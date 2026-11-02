#ifndef __AURA_MOTION_H__
#define __AURA_MOTION_H__

#include "config.h"
#include "mcp_server.h"
#include "pca9685.h"
#include "settings.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>

// Body motion for AURA-902: 4 legs (one MG90S per leg, swinging fore/aft) and a pan/tilt head,
// driven through a PCA9685 on the codec I2C bus. All motion runs in its own low-priority task;
// callers only post commands, so the main loop, audio and LVGL never block on servos.
//
// Logical angles are degrees from the calibrated centre: legs + = forward, pan + = left,
// tilt + = up. Mounting direction is handled by AURA_SERVO_INVERT_* and per-servo trims (NVS).
class AuraMotion {
public:
    enum Servo { kFrontLeft, kFrontRight, kRearLeft, kRearRight, kPan, kTilt, kServoCount };

private:
    static constexpr const char* kTag = "AuraMotion";
    static constexpr int kStepMs = 20;              // interpolation step (servo frame)
    static constexpr int64_t kRelaxAfterUs = 3000000;  // power servos down after 3 s idle
    static constexpr int64_t kReactionGapUs = 4000000; // min time between mood reactions

    enum CmdType : uint8_t { kCmdGesture, kCmdWalk, kCmdHead, kCmdReaction };
    struct Cmd {
        CmdType type;
        int8_t a;  // gesture id / direction / pan
        int8_t b;  // steps / tilt
    };

    struct ServoConfig {
        uint8_t channel;
        bool invert;
        int min_angle;
        int max_angle;
    };
    static constexpr ServoConfig kServos[kServoCount] = {
        {AURA_SERVO_CH_FRONT_LEFT, AURA_SERVO_INVERT_FRONT_LEFT, -AURA_LEG_LIMIT, AURA_LEG_LIMIT},
        {AURA_SERVO_CH_FRONT_RIGHT, AURA_SERVO_INVERT_FRONT_RIGHT, -AURA_LEG_LIMIT, AURA_LEG_LIMIT},
        {AURA_SERVO_CH_REAR_LEFT, AURA_SERVO_INVERT_REAR_LEFT, -AURA_LEG_LIMIT, AURA_LEG_LIMIT},
        {AURA_SERVO_CH_REAR_RIGHT, AURA_SERVO_INVERT_REAR_RIGHT, -AURA_LEG_LIMIT, AURA_LEG_LIMIT},
        {AURA_SERVO_CH_PAN, AURA_SERVO_INVERT_PAN, -AURA_PAN_LIMIT, AURA_PAN_LIMIT},
        {AURA_SERVO_CH_TILT, AURA_SERVO_INVERT_TILT, -AURA_TILT_LIMIT, AURA_TILT_LIMIT},
    };

    Pca9685 pca_;
    QueueHandle_t queue_ = nullptr;
    std::atomic<bool> stop_{false};
    std::atomic<bool> busy_{false};
    bool ready_ = false;
    bool relaxed_ = true;
    float current_[kServoCount] = {};
    int trim_[kServoCount] = {};
    int64_t last_motion_us_ = 0;
    int64_t last_reaction_us_ = 0;
    int pose_ = 0;  // last named pose (gesture id), used to stand up after sleeping

    // ------------------------------------------------------------ low level
    void Write(int servo, float angle) {
        const ServoConfig& cfg = kServos[servo];
        angle = std::clamp(angle, (float)cfg.min_angle, (float)cfg.max_angle);
        float a = (angle + trim_[servo]) * (cfg.invert ? -1.0f : 1.0f);
        constexpr float kCenterUs = (AURA_SERVO_PULSE_MIN_US + AURA_SERVO_PULSE_MAX_US) / 2.0f;
        constexpr float kUsPerDeg = (float)(AURA_SERVO_PULSE_MAX_US - AURA_SERVO_PULSE_MIN_US) / AURA_SERVO_RANGE_DEG;
        uint32_t pulse = (uint32_t)lroundf(kCenterUs + a * kUsPerDeg);
        pca_.SetPulseUs(cfg.channel, std::clamp<uint32_t>(pulse, AURA_SERVO_PULSE_MIN_US, AURA_SERVO_PULSE_MAX_US));
    }

    // Moves the given servos (NAN = keep) to their targets in `ms`, linearly. Returns false if
    // interrupted by Stop().
    bool MoveTo(const float target[kServoCount], int ms) {
        if (relaxed_) {  // re-energize at the last known positions before moving
            for (int i = 0; i < kServoCount; ++i) {
                Write(i, current_[i]);
            }
            relaxed_ = false;
        }
        float start[kServoCount];
        memcpy(start, current_, sizeof(start));
        int steps = std::max(1, ms / kStepMs);
        for (int s = 1; s <= steps; ++s) {
            if (stop_) {
                return false;
            }
            float k = (float)s / steps;
            for (int i = 0; i < kServoCount; ++i) {
                if (!std::isnan(target[i])) {
                    current_[i] = start[i] + (target[i] - start[i]) * k;
                    Write(i, current_[i]);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(kStepMs));
        }
        last_motion_us_ = esp_timer_get_time();
        return true;
    }

    bool Legs(float fl, float fr, float rl, float rr, int ms) {
        const float t[kServoCount] = {fl, fr, rl, rr, NAN, NAN};
        return MoveTo(t, ms);
    }
    bool Head(float pan, float tilt, int ms) {
        const float t[kServoCount] = {NAN, NAN, NAN, NAN, pan, tilt};
        return MoveTo(t, ms);
    }
    bool Leg(int servo, float angle, int ms) {
        float t[kServoCount] = {NAN, NAN, NAN, NAN, NAN, NAN};
        t[servo] = angle;
        return MoveTo(t, ms);
    }
    bool Hold(int ms) {
        for (int t = 0; t < ms; t += kStepMs) {
            if (stop_) {
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(kStepMs));
        }
        last_motion_us_ = esp_timer_get_time();
        return true;
    }

    // ------------------------------------------------------------ gestures
public:
    enum Gesture : int8_t {
        G_STAND, G_SIT, G_LIE, G_BOW, G_NOD, G_SHAKE_HEAD, G_LOOK_LEFT, G_LOOK_RIGHT,
        G_LOOK_UP, G_LOOK_DOWN, G_WAVE, G_DANCE, G_STRETCH, G_WIGGLE, G_COUNT
    };

private:
    static constexpr const char* kGestureNames[G_COUNT] = {
        "stand", "sit", "lie", "bow", "nod", "shake_head", "look_left", "look_right",
        "look_up", "look_down", "wave", "dance", "stretch", "wiggle",
    };

    bool RunGesture(int g) {
        bool ok = true;
        switch (g) {
        case G_STAND:
            ok = Legs(0, 0, 0, 0, 500) && Head(0, 0, 300);
            break;
        case G_SIT:  // rear legs forward under the body lower the back
            ok = Legs(0, 0, 55, 55, 700) && Head(0, 15, 300);
            break;
        case G_LIE:  // front legs forward, rear legs back: belly down
            ok = Legs(65, 65, -65, -65, 900) && Head(0, -20, 400);
            break;
        case G_BOW:
            ok = Legs(50, 50, 0, 0, 500) && Head(0, -25, 300) && Hold(800) && Legs(0, 0, 0, 0, 500) &&
                 Head(0, 0, 300);
            break;
        case G_NOD:
            for (int i = 0; i < 3 && ok; ++i) {
                ok = Head(NAN, 15, 180) && Head(NAN, -15, 180);
            }
            ok = ok && Head(NAN, 0, 150);
            break;
        case G_SHAKE_HEAD:
            for (int i = 0; i < 3 && ok; ++i) {
                ok = Head(30, NAN, 200) && Head(-30, NAN, 200);
            }
            ok = ok && Head(0, NAN, 150);
            break;
        case G_LOOK_LEFT:
        case G_LOOK_RIGHT:
            ok = Head(g == G_LOOK_LEFT ? 45 : -45, 5, 400) && Hold(1200) && Head(0, 0, 400);
            break;
        case G_LOOK_UP:
        case G_LOOK_DOWN:
            ok = Head(0, g == G_LOOK_UP ? 25 : -25, 400) && Hold(1200) && Head(0, 0, 400);
            break;
        case G_WAVE:  // three legs hold, front-left paw waves
            ok = Legs(0, -10, 10, 10, 300) && Leg(kFrontLeft, 75, 300);
            for (int i = 0; i < 3 && ok; ++i) {
                ok = Leg(kFrontLeft, 50, 200) && Leg(kFrontLeft, 75, 200);
            }
            ok = ok && Legs(0, 0, 0, 0, 400);
            break;
        case G_DANCE:
            for (int i = 0; i < 4 && ok; ++i) {
                const float l[kServoCount] = {20, -20, 20, -20, -20, 10};
                const float r[kServoCount] = {-20, 20, -20, 20, 20, -10};
                ok = MoveTo(l, 300) && MoveTo(r, 300);
            }
            ok = ok && Legs(0, 0, 0, 0, 300) && Head(0, 0, 200);
            break;
        case G_STRETCH:
            ok = Legs(50, 50, -50, -50, 800) && Head(0, 20, 300) && Hold(1000) && Legs(0, 0, 0, 0, 700) &&
                 Head(0, 0, 300);
            break;
        case G_WIGGLE:
            for (int i = 0; i < 3 && ok; ++i) {
                ok = Head(15, 5, 140) && Head(-15, 5, 140);
            }
            ok = ok && Head(0, 0, 140);
            break;
        }
        pose_ = g;
        return ok;
    }

    // Stick-slip creep for one-DOF legs: each leg swings alone (it carries little weight, so it
    // slides), then all four push together (high grip) and the body moves. For turns the two sides
    // swing in opposite directions. direction: 0 forward, 1 backward, 2 left, 3 right.
    bool Walk(int direction, int steps) {
        const int a = AURA_GAIT_AMPLITUDE;
        int sign[4];  // per leg: + swing forward, - swing backward
        switch (direction) {
        case 0: sign[0] = sign[1] = sign[2] = sign[3] = 1; break;
        case 1: sign[0] = sign[1] = sign[2] = sign[3] = -1; break;
        case 2: sign[kFrontLeft] = sign[kRearLeft] = -1; sign[kFrontRight] = sign[kRearRight] = 1; break;
        default: sign[kFrontLeft] = sign[kRearLeft] = 1; sign[kFrontRight] = sign[kRearRight] = -1; break;
        }
        static constexpr int kOrder[4] = {kFrontLeft, kRearRight, kFrontRight, kRearLeft};
        bool ok = Legs(0, 0, 0, 0, 300);
        for (int s = 0; s < steps && ok; ++s) {
            for (int i = 0; i < 4 && ok; ++i) {
                int leg = kOrder[i];
                ok = Leg(leg, sign[leg] * a, AURA_GAIT_SWING_MS);
            }
            ok = ok && Legs(0, 0, 0, 0, AURA_GAIT_PUSH_MS);
        }
        pose_ = G_STAND;
        return ok;
    }

    static int GestureForMood(int mood) {
        switch (mood) {
        case 1: case 26: case 33: case 99: return G_WIGGLE;              // feliz
        case 3: case 10: case 60: case 80: case 83: case 100: return G_DANCE;  // emocionado/riendo
        case 6: case 27: case 79: case 84: return G_LOOK_DOWN;           // triste/llorando
        case 7: case 42: case 58: return G_SHAKE_HEAD;                   // enojado
        case 2: case 15: case 81: return G_LOOK_UP;                      // sorprendido
        case 8: case 14: case 17: return G_LOOK_LEFT;                    // curioso/pensativo
        case 11: case 54: case 55: return G_NOD;                         // enamorado/cariñoso
        case 9: return G_LIE;                                            // dormido
        default: return -1;
        }
    }

    void Execute(const Cmd& cmd) {
        busy_ = true;
        stop_ = false;
        switch (cmd.type) {
        case kCmdGesture:
        case kCmdReaction:
            RunGesture(cmd.a);
            break;
        case kCmdWalk:
            Walk(cmd.a, cmd.b);
            break;
        case kCmdHead:
            Head(cmd.a, cmd.b, 400);
            break;
        }
        busy_ = false;
    }

    static void TaskCb(void* arg) {
        auto* self = static_cast<AuraMotion*>(arg);
        Cmd cmd;
        while (true) {
            if (xQueueReceive(self->queue_, &cmd, pdMS_TO_TICKS(200)) == pdTRUE) {
                self->Execute(cmd);
            } else if (!self->relaxed_ && esp_timer_get_time() - self->last_motion_us_ > kRelaxAfterUs) {
                self->pca_.AllOff();  // no holding torque at rest: no jitter, heat or battery drain
                self->relaxed_ = true;
            }
        }
    }

    bool Post(const Cmd& cmd) {
        if (!ready_) {
            return false;
        }
        return xQueueSend(queue_, &cmd, 0) == pdTRUE;
    }

    void RegisterTools() {
        auto& mcp = McpServer::GetInstance();
        std::string names;
        for (int i = 0; i < G_COUNT; ++i) {
            names += (i ? ", " : "") + std::string(kGestureNames[i]);
        }
        mcp.AddTool("self.robot.gesture",
            "Make the robot body move: a pose or gesture with its 4 legs and head. Use it when the user asks "
            "the robot to sit, lie down, wave, dance, bow, nod, look somewhere, etc. `name` is one of: " + names,
            PropertyList({Property("name", kPropertyTypeString)}),
            [this](const PropertyList& p) -> ReturnValue {
                int g = GestureId(p["name"].value<std::string>());
                if (g < 0) {
                    return "{\"success\": false, \"message\": \"Unknown gesture\"}";
                }
                return Post({kCmdGesture, (int8_t)g, 0});
            });
        mcp.AddTool("self.robot.walk",
            "Make the robot walk. `direction`: forward, backward, left (turn left) or right (turn right). "
            "`steps`: number of steps (1-10).",
            PropertyList({Property("direction", kPropertyTypeString), Property("steps", kPropertyTypeInteger, 3, 1, 10)}),
            [this](const PropertyList& p) -> ReturnValue {
                std::string d = p["direction"].value<std::string>();
                int dir = d == "forward" ? 0 : d == "backward" ? 1 : d == "left" ? 2 : d == "right" ? 3 : -1;
                if (dir < 0) {
                    return "{\"success\": false, \"message\": \"Unknown direction\"}";
                }
                return Post({kCmdWalk, (int8_t)dir, (int8_t)p["steps"].value<int>()});
            });
        mcp.AddTool("self.robot.head",
            "Point the robot's head. `pan`: -60 (right) to 60 (left) degrees; `tilt`: -35 (down) to 35 (up).",
            PropertyList({Property("pan", kPropertyTypeInteger, 0, -AURA_PAN_LIMIT, AURA_PAN_LIMIT),
                          Property("tilt", kPropertyTypeInteger, 0, -AURA_TILT_LIMIT, AURA_TILT_LIMIT)}),
            [this](const PropertyList& p) -> ReturnValue {
                return Post({kCmdHead, (int8_t)p["pan"].value<int>(), (int8_t)p["tilt"].value<int>()});
            });
        mcp.AddTool("self.robot.stop", "Stop any robot movement immediately.", PropertyList(),
            [this](const PropertyList& p) -> ReturnValue {
                Stop();
                return true;
            });
        mcp.AddTool("self.robot.calibrate",
            "Adjust the centre (trim) of one servo and move it to its centre, saved permanently. Servos: "
            "0 front-left leg, 1 front-right leg, 2 rear-left leg, 3 rear-right leg, 4 head pan, 5 head tilt. "
            "`trim` in degrees (-30..30). Only use when the user explicitly calibrates the robot.",
            PropertyList({Property("servo", kPropertyTypeInteger, 0, 0, kServoCount - 1),
                          Property("trim", kPropertyTypeInteger, 0, -30, 30)}),
            [this](const PropertyList& p) -> ReturnValue {
                SetTrim(p["servo"].value<int>(), p["trim"].value<int>());
                return Post({kCmdGesture, G_STAND, 0});
            });
    }

    static int GestureId(const std::string& name) {
        for (int i = 0; i < G_COUNT; ++i) {
            if (name == kGestureNames[i]) {
                return i;
            }
        }
        return -1;
    }

public:
    // Returns false (and stays inert) if no PCA9685 is found; the robot then works without a body.
    bool Init(i2c_master_bus_handle_t bus) {
        if (!pca_.Init(bus, AURA_PCA9685_ADDR)) {
            return false;
        }
        Settings settings("aura_servo");
        for (int i = 0; i < kServoCount; ++i) {
            trim_[i] = settings.GetInt("trim" + std::to_string(i), 0);
        }
        queue_ = xQueueCreate(4, sizeof(Cmd));
        ready_ = queue_ != nullptr &&
                 xTaskCreate(TaskCb, "aura_motion", 4096, this, 3, nullptr) == pdPASS;
        if (ready_) {
            RegisterTools();
        }
        ESP_LOGI(kTag, "Motion %s", ready_ ? "ready" : "failed");
        return ready_;
    }

    bool IsReady() const { return ready_; }

    // Interrupts the running movement and drops queued ones.
    void Stop() {
        if (!ready_) {
            return;
        }
        xQueueReset(queue_);
        stop_ = true;
    }

    // Small body reaction to a face change. Non-blocking (safe from the LVGL task); ignored while a
    // requested movement runs and rate-limited so the robot does not fidget constantly.
    void ReactToMood(int mood) {
        if (!ready_ || busy_ || uxQueueMessagesWaiting(queue_) > 0) {
            return;
        }
        int64_t now = esp_timer_get_time();
        int g = GestureForMood(mood);
        if (g < 0 && mood == 4 && (pose_ == G_LIE || pose_ == G_SIT)) {
            g = G_STAND;  // waking up from sleep
        }
        if (g < 0 || now - last_reaction_us_ < kReactionGapUs) {
            return;
        }
        last_reaction_us_ = now;
        Post({kCmdReaction, (int8_t)g, 0});
    }

    void SetTrim(int servo, int trim) {
        if (servo < 0 || servo >= kServoCount) {
            return;
        }
        trim_[servo] = std::clamp(trim, -30, 30);
        Settings settings("aura_servo", true);
        settings.SetInt("trim" + std::to_string(servo), trim_[servo]);
        ESP_LOGI(kTag, "Servo %d trim %d", servo, trim_[servo]);
    }
};

#endif // __AURA_MOTION_H__
