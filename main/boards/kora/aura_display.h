#ifndef __AURA_DISPLAY_H__
#define __AURA_DISPLAY_H__

#include "application.h"
#include "config.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <lvgl.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>

// SpiLcdDisplay with a kid-friendly animated robot face (navy background, blue strokes, red eyes
// when angry) drawn with LVGL primitives instead of emoji, 100 moods built from parts, plus a
// temperature/humidity badge at the bottom of the round screen.
class AuraDisplay : public SpiLcdDisplay {
public:
    static constexpr int kMoodCount = 100;

private:
    // ---------------------------------------------------------------- mood parts
    enum Eye : uint8_t {
        E_ARC,         // ^ closed happy
        E_ARC_DOWN,    // closed relaxed
        E_SQUARE,      // rounded square with pupil
        E_CHEVRON,     // > <
        E_LINE,        // - closed
        E_LINE_SHORT,  // short -
        E_SAD,         // droopy lid
        E_ANGRY,       // red, slanted lid
        E_HALF,        // blue, slanted lid (determined / suspicious)
        E_HEART,
        E_STAR,
        E_DOT,         // big round
    };
    enum Mouth : uint8_t {
        M_BIG, M_HUGE, M_SMILE, M_SMALL_SMILE, M_SMIRK, M_O, M_O_SMALL, M_DOT,
        M_FLAT, M_FLAT_SHORT, M_FROWN, M_SMALL_FROWN, M_WAVY, M_TONGUE, M_OPEN_SAD,
    };
    enum Extra : uint8_t {
        X_NONE = 0, X_ZZZ = 1, X_TEAR = 2, X_SWEAT = 4, X_BLUSH = 8, X_SPARKLE = 16, X_BROWS = 32,
    };
    enum Anim : uint8_t {
        A_NONE, A_BOUNCE, A_BREATHE, A_SHAKE, A_TREMBLE, A_PULSE, A_LOOK, A_FLOATZ, A_TEAR,
    };

    struct Mood {
        const char* name;
        Eye eye_l;
        Eye eye_r;
        int8_t pupil_dx;
        int8_t pupil_dy;
        Mouth mouth;
        uint8_t extras;
        Anim anim;
    };

    // Index + 1 is the number shown in the AURA-902 mood sheet.
    static constexpr Mood kMoods[kMoodCount] = {
        {"Feliz",         E_ARC,     E_ARC,     0, 0, M_BIG,         X_NONE,    A_BOUNCE},   // 1
        {"Sorprendido",   E_SQUARE,  E_SQUARE,  0, 0, M_O,           X_NONE,    A_NONE},     // 2
        {"Emocionado",    E_CHEVRON, E_CHEVRON, 0, 0, M_BIG,         X_NONE,    A_BOUNCE},   // 3
        {"Sereno",        E_LINE,    E_LINE,    0, 0, M_SMILE,       X_NONE,    A_BREATHE},  // 4
        {"Travieso",      E_LINE,    E_SQUARE,  0, 0, M_TONGUE,      X_NONE,    A_NONE},     // 5
        {"Triste",        E_SAD,     E_SAD,     0, 0, M_FROWN,       X_NONE,    A_BREATHE},  // 6
        {"Enojado",       E_ANGRY,   E_ANGRY,   0, 0, M_FROWN,       X_NONE,    A_SHAKE},    // 7
        {"Curioso",       E_SQUARE,  E_SQUARE,  7, 0, M_O_SMALL,     X_NONE,    A_LOOK},     // 8
        {"Dormido",       E_LINE,    E_LINE,    0, 0, M_DOT,         X_ZZZ,     A_FLOATZ},   // 9
        {"Divertido",     E_CHEVRON, E_CHEVRON, 0, 0, M_HUGE,        X_NONE,    A_BOUNCE},   // 10
        {"Enamorado",     E_HEART,   E_HEART,   0, 0, M_BIG,         X_NONE,    A_PULSE},    // 11
        {"Dudoso",        E_LINE,    E_SQUARE,  0, 0, M_SMIRK,       X_NONE,    A_NONE},     // 12
        {"Preocupado",    E_SQUARE,  E_SQUARE,  0, 3, M_O_SMALL,     X_BROWS,   A_TREMBLE},  // 13
        {"Pensativo",     E_HALF,    E_HALF,    0, 0, M_WAVY,        X_NONE,    A_BREATHE},  // 14
        {"Asombrado",     E_SQUARE,  E_SQUARE,  0, 0, M_DOT,         X_NONE,    A_NONE},     // 15
        {"Guiño",         E_CHEVRON, E_SQUARE,  0, 0, M_SMIRK,       X_NONE,    A_NONE},     // 16
        {"Confundido",    E_SQUARE,  E_SQUARE,  0, 0, M_WAVY,        X_NONE,    A_LOOK},     // 17
        {"Aliviado",      E_ARC,     E_ARC,     0, 0, M_BIG,         X_NONE,    A_BREATHE},  // 18
        {"Concentrado",   E_HALF,    E_HALF,    0, 0, M_FLAT,        X_NONE,    A_NONE},     // 19
        {"Entusiasmado",  E_SQUARE,  E_SQUARE,  0, 0, M_BIG,         X_SPARKLE, A_BOUNCE},   // 20
        {"Indeciso",      E_SQUARE,  E_LINE,    0, 0, M_SMALL_FROWN, X_NONE,    A_LOOK},     // 21
        {"Agradecido",    E_ARC,     E_ARC,     0, 0, M_BIG,         X_NONE,    A_BREATHE},  // 22
        {"Nervioso",      E_SQUARE,  E_SQUARE,  0, 0, M_SMALL_SMILE, X_SWEAT,   A_TREMBLE},  // 23
        {"Relajado",      E_ARC_DOWN, E_LINE,   0, 0, M_SMALL_SMILE, X_NONE,    A_BREATHE},  // 24
        {"Cansado",       E_LINE,    E_SQUARE,  0, 3, M_SMALL_FROWN, X_NONE,    A_BREATHE},  // 25
        {"Motivado",      E_ARC,     E_ARC,     0, 0, M_BIG,         X_NONE,    A_BOUNCE},   // 26
        {"Decepcionado",  E_SAD,     E_SAD,     0, 0, M_FROWN,       X_NONE,    A_BREATHE},  // 27
        {"Orgulloso",     E_CHEVRON, E_CHEVRON, 0, 0, M_BIG,         X_SPARKLE, A_BOUNCE},   // 28
        {"Vergonzoso",    E_SQUARE,  E_SQUARE,  0, 2, M_FLAT_SHORT,  X_BLUSH,   A_NONE},     // 29
        {"Tímido",        E_LINE,    E_LINE,    0, 0, M_SMALL_SMILE, X_BLUSH,   A_BREATHE},  // 30
        {"Impaciente",    E_CHEVRON, E_CHEVRON, 0, 0, M_DOT,         X_NONE,    A_SHAKE},    // 31
        {"Tranquilo",     E_LINE,    E_LINE,    0, 0, M_SMILE,       X_NONE,    A_BREATHE},  // 32
        {"Alegre",        E_ARC,     E_ARC,     0, 0, M_BIG,         X_NONE,    A_BOUNCE},   // 33
        {"Aburrido",      E_LINE,    E_LINE,    0, 0, M_FLAT,        X_NONE,    A_BREATHE},  // 34
        {"Interesado",    E_SQUARE,  E_SQUARE,  0, -4, M_SMALL_SMILE, X_NONE,   A_LOOK},     // 35
        {"Intrigado",     E_SQUARE,  E_SQUARE,  0, 0, M_O,           X_NONE,    A_NONE},     // 36
        {"Pensativo 2",   E_SQUARE,  E_SQUARE, -7, 0, M_SMALL_FROWN, X_NONE,    A_LOOK},     // 37
        {"Sospechoso",    E_HALF,    E_SQUARE,  5, 0, M_FLAT,        X_SWEAT,   A_LOOK},     // 38
        {"Asustado",      E_SQUARE,  E_SQUARE,  0, 0, M_OPEN_SAD,    X_BROWS,   A_TREMBLE},  // 39
        {"Valiente",      E_HALF,    E_HALF,    0, 0, M_BIG,         X_SWEAT,   A_BOUNCE},   // 40
        {"Satisfecho",    E_SQUARE,  E_SQUARE,  0, 0, M_SMALL_SMILE, X_NONE,    A_NONE},     // 41
        {"Frustrado",     E_ANGRY,   E_ANGRY,   0, 0, M_FROWN,       X_NONE,    A_SHAKE},    // 42
        {"Incrédulo",     E_SQUARE,  E_SQUARE,  0, 0, M_SMILE,       X_NONE,    A_NONE},     // 43
        {"Admirado",      E_STAR,    E_STAR,    0, 0, M_O,           X_NONE,    A_PULSE},    // 44
        {"Celoso",        E_STAR,    E_STAR,    0, 0, M_FLAT_SHORT,  X_NONE,    A_PULSE},    // 45
        {"Determinado",   E_CHEVRON, E_CHEVRON, 0, 0, M_BIG,         X_NONE,    A_BOUNCE},   // 46
        {"Alerta",        E_SQUARE,  E_SQUARE,  0, 0, M_SMILE,       X_NONE,    A_LOOK},     // 47
        {"Distraído",     E_SQUARE,  E_SQUARE,  0, 4, M_WAVY,        X_NONE,    A_LOOK},     // 48
        {"Apático",       E_LINE,    E_LINE,    0, 0, M_FLAT_SHORT,  X_NONE,    A_NONE},     // 49
        {"Optimista",     E_ARC,     E_ARC,     0, 0, M_BIG,         X_SPARKLE, A_BOUNCE},   // 50
        {"Pesimista",     E_LINE,    E_LINE,    0, 0, M_FROWN,       X_NONE,    A_BREATHE},  // 51
        {"Confidente",    E_LINE,    E_SQUARE,  0, 0, M_SMIRK,       X_BLUSH,   A_NONE},     // 52
        {"Cómplice",      E_ARC,     E_CHEVRON, 0, 0, M_BIG,         X_BLUSH,   A_BOUNCE},   // 53
        {"Cariñoso",      E_HEART,   E_HEART,   0, 0, M_SMALL_SMILE, X_NONE,    A_PULSE},    // 54
        {"Romántico",     E_HEART,   E_HEART,   0, 0, M_BIG,         X_BLUSH,   A_PULSE},    // 55
        {"Ilusionado",    E_CHEVRON, E_CHEVRON, 0, 0, M_BIG,         X_BLUSH,   A_BOUNCE},   // 56
        {"Melancólico",   E_DOT,     E_DOT,     0, 0, M_SMALL_FROWN, X_NONE,    A_BREATHE},  // 57
        {"Enojado 2",     E_ANGRY,   E_ANGRY,   0, 0, M_OPEN_SAD,    X_NONE,    A_SHAKE},    // 58
        {"Tranquilo 2",   E_LINE,    E_LINE,    0, 0, M_FLAT_SHORT,  X_NONE,    A_BREATHE},  // 59
        {"Genial",        E_CHEVRON, E_CHEVRON, 0, 0, M_HUGE,        X_SPARKLE, A_BOUNCE},   // 60
        {"Sorprendido 2", E_SQUARE,  E_SQUARE,  0, 0, M_SMALL_SMILE, X_NONE,    A_NONE},     // 61
        {"Ofendido",      E_ANGRY,   E_ANGRY,   0, 0, M_WAVY,        X_SWEAT,   A_SHAKE},    // 62
        {"Complacido",    E_ARC,     E_ARC,     0, 0, M_BIG,         X_NONE,    A_BREATHE},  // 63
        {"Pensando",      E_LINE,    E_SQUARE,  0, -4, M_DOT,        X_NONE,    A_LOOK},     // 64
        {"Con sueño",     E_ARC_DOWN, E_LINE_SHORT, 0, 0, M_SMALL_SMILE, X_ZZZ, A_FLOATZ},   // 65
        {"Recuperado",    E_SQUARE,  E_SQUARE,  0, 0, M_SMIRK,       X_NONE,    A_NONE},     // 66
        {"Seguro",        E_LINE,    E_LINE,    0, 0, M_SMILE,       X_NONE,    A_BREATHE},  // 67
        {"Inseguro",      E_SQUARE,  E_SQUARE, -6, 0, M_SMALL_SMILE, X_SWEAT,   A_LOOK},     // 68
        {"Expectante",    E_SQUARE,  E_SQUARE,  0, 0, M_O_SMALL,     X_NONE,    A_NONE},     // 69
        {"Encantado",     E_ARC,     E_ARC,     0, 0, M_BIG,         X_BLUSH,   A_BOUNCE},   // 70
        {"Pasivo",        E_LINE,    E_LINE,    0, 0, M_FLAT,        X_NONE,    A_NONE},     // 71
        {"Activo",        E_HALF,    E_HALF,    0, 0, M_HUGE,        X_SWEAT,   A_BOUNCE},   // 72
        {"Indignado",     E_ANGRY,   E_ANGRY,   0, 0, M_OPEN_SAD,    X_NONE,    A_SHAKE},    // 73
        {"Dolido",        E_LINE,    E_LINE,    0, 0, M_O_SMALL,     X_BROWS,   A_BREATHE},  // 74
        {"Esperanzado",   E_ARC,     E_ARC,     0, 0, M_BIG,         X_NONE,    A_BREATHE},  // 75
        {"Negativo",      E_LINE,    E_LINE,    0, 0, M_SMALL_FROWN, X_NONE,    A_NONE},     // 76
        {"Positivo",      E_LINE,    E_LINE,    0, 0, M_BIG,         X_NONE,    A_BOUNCE},   // 77
        {"Extrañado",     E_SQUARE,  E_SQUARE,  0, 0, M_SMALL_SMILE, X_NONE,    A_LOOK},     // 78
        {"Entristecido",  E_LINE,    E_LINE,    0, 0, M_SMALL_FROWN, X_TEAR,    A_TEAR},     // 79
        {"Emocionado 2",  E_CHEVRON, E_CHEVRON, 0, 0, M_HUGE,        X_SPARKLE, A_BOUNCE},   // 80
        {"Asombrado 2",   E_SQUARE,  E_SQUARE, -5, 0, M_O,           X_NONE,    A_NONE},     // 81
        {"Sereno 2",      E_LINE,    E_LINE,    0, 0, M_SMILE,       X_NONE,    A_BREATHE},  // 82
        {"Riendo",        E_ARC,     E_ARC,     0, 0, M_HUGE,        X_NONE,    A_BOUNCE},   // 83
        {"Llorando",      E_DOT,     E_DOT,     0, 0, M_OPEN_SAD,    X_TEAR,    A_TEAR},     // 84
        {"Conmovido",     E_SQUARE,  E_SQUARE,  0, 0, M_SMALL_SMILE, X_TEAR,    A_TEAR},     // 85
        {"Avergonzado",   E_SQUARE,  E_SQUARE,  0, 3, M_SMALL_SMILE, X_BLUSH,   A_NONE},     // 86
        {"Concentrado 2", E_HALF,    E_HALF,    0, 0, M_FLAT_SHORT,  X_NONE,    A_NONE},     // 87
        {"Atento",        E_SQUARE,  E_SQUARE,  0, 0, M_SMALL_SMILE, X_NONE,    A_LOOK},     // 88
        {"Inspirado",     E_SQUARE,  E_SQUARE,  0, -4, M_SMILE,      X_SPARKLE, A_PULSE},    // 89
        {"Determinado 2", E_HALF,    E_HALF,    0, 0, M_BIG,         X_NONE,    A_BOUNCE},   // 90
        {"Divertido 2",   E_CHEVRON, E_CHEVRON, 0, 0, M_BIG,         X_NONE,    A_BOUNCE},   // 91
        {"Triste 2",      E_LINE,    E_LINE,    0, 0, M_SMALL_FROWN, X_BROWS,   A_BREATHE},  // 92
        {"Desconfiado",   E_HALF,    E_HALF,    6, 0, M_SMALL_FROWN, X_NONE,    A_LOOK},     // 93
        {"Intrigado 2",   E_SQUARE,  E_SQUARE,  0, 0, M_SMILE,       X_SWEAT,   A_NONE},     // 94
        {"Cansado 2",     E_LINE,    E_LINE,    0, 0, M_TONGUE,      X_NONE,    A_BREATHE},  // 95
        {"Apasionado",    E_HEART,   E_HEART,   0, 0, M_SMALL_SMILE, X_SPARKLE, A_PULSE},    // 96
        {"Calmado",       E_LINE,    E_LINE,    0, 0, M_FLAT,        X_NONE,    A_BREATHE},  // 97
        {"Impresionado",  E_SQUARE,  E_SQUARE,  0, -3, M_O,          X_NONE,    A_NONE},     // 98
        {"Feliz 2",       E_ARC,     E_ARC,     0, 0, M_BIG,         X_NONE,    A_BOUNCE},   // 99
        {"Celebrando",    E_DOT,     E_DOT,     0, 0, M_HUGE,        X_SPARKLE, A_BOUNCE},   // 100
    };

    // ---------------------------------------------------------------- colours and layout
    static constexpr uint32_t kNavy = 0x07111F;
    static constexpr uint32_t kBlue = 0x2F7DFF;  // main stroke colour
    static constexpr uint32_t kRed = 0xFF3B30;   // angry eyes
    static constexpr int kLeftEyeX = 85;
    static constexpr int kRightEyeX = 155;
    static constexpr int kEyeY = 100;
    static constexpr int kMouthX = 120;
    static constexpr uint32_t kFramePeriodMs = 125;  // 8 fps keeps SPI load low
    static constexpr uint32_t kBlinkEveryTicks = 28; // ~3.5 s

    lv_obj_t* face_obj_ = nullptr;
    lv_obj_t* env_label_ = nullptr;
    lv_timer_t* anim_timer_ = nullptr;
    int mood_ = 3;  // index into kMoods ("Sereno")
    uint32_t tick_ = 0;
    uint32_t blink_tick_ = 0;
    bool blinking_ = false;
    bool speaking_ = false;      // mouth animates while the assistant talks
    int64_t hold_until_us_ = 0;  // a mood requested by voice wins over server emotions until then
    int64_t idle_since_us_ = 0;  // when the device last entered the idle state
    bool sleeping_ = false;      // showing "Dormido" after AURA_SLEEP_AFTER_S idle seconds
    bool greeted_ = false;       // "Feliz" greeting shown once when startup finishes
    std::function<void(int)> on_mood_changed_;  // e.g. body reaction; must not block

    // ---------------------------------------------------------------- drawing primitives
    struct Canvas {
        lv_layer_t* layer;
        int x0;
        int y0;
    };

    static void Line(const Canvas& c, int x1, int y1, int x2, int y2, int width, uint32_t color = kBlue) {
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = lv_color_hex(color);
        dsc.width = width;
        dsc.round_start = 1;
        dsc.round_end = 1;
        dsc.p1.x = c.x0 + x1;
        dsc.p1.y = c.y0 + y1;
        dsc.p2.x = c.x0 + x2;
        dsc.p2.y = c.y0 + y2;
        lv_draw_line(c.layer, &dsc);
    }

    // Angles in degrees, 0 = 3 o'clock, clockwise. width == radius gives a filled sector.
    static void Arc(const Canvas& c, int cx, int cy, int radius, int start, int end, int width,
                    uint32_t color = kBlue) {
        lv_draw_arc_dsc_t dsc;
        lv_draw_arc_dsc_init(&dsc);
        dsc.color = lv_color_hex(color);
        dsc.center.x = c.x0 + cx;
        dsc.center.y = c.y0 + cy;
        dsc.radius = radius;
        dsc.start_angle = start;
        dsc.end_angle = end;
        dsc.width = width;
        dsc.rounded = width < radius ? 1 : 0;
        lv_draw_arc(c.layer, &dsc);
    }

    static void Rect(const Canvas& c, int x, int y, int w, int h, int radius, uint32_t fill,
                     int border = 0, uint32_t border_color = kBlue) {
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.radius = radius;
        if (fill == UINT32_MAX) {
            dsc.bg_opa = LV_OPA_TRANSP;
        } else {
            dsc.bg_color = lv_color_hex(fill);
        }
        dsc.border_width = border;
        dsc.border_color = lv_color_hex(border_color);
        lv_area_t area = {c.x0 + x, c.y0 + y, c.x0 + x + w - 1, c.y0 + y + h - 1};
        lv_draw_rect(c.layer, &dsc, &area);
    }

    static void Dot(const Canvas& c, int cx, int cy, int r, uint32_t color = kBlue) {
        Rect(c, cx - r, cy - r, 2 * r, 2 * r, LV_RADIUS_CIRCLE, color);
    }

    static void Triangle(const Canvas& c, int x1, int y1, int x2, int y2, int x3, int y3,
                         uint32_t color = kBlue) {
        lv_draw_triangle_dsc_t dsc;
        lv_draw_triangle_dsc_init(&dsc);
        dsc.color = lv_color_hex(color);
        dsc.p[0].x = c.x0 + x1;
        dsc.p[0].y = c.y0 + y1;
        dsc.p[1].x = c.x0 + x2;
        dsc.p[1].y = c.y0 + y2;
        dsc.p[2].x = c.x0 + x3;
        dsc.p[2].y = c.y0 + y3;
        lv_draw_triangle(c.layer, &dsc);
    }

    static void Heart(const Canvas& c, int cx, int cy, int s) {
        Dot(c, cx - s / 2, cy - s / 4, s / 2 + 1);
        Dot(c, cx + s / 2, cy - s / 4, s / 2 + 1);
        Triangle(c, cx - s - 1, cy - s / 8, cx + s + 1, cy - s / 8, cx, cy + s);
    }

    static void Star(const Canvas& c, int cx, int cy, int r) {
        constexpr float kDeg = 3.14159265f / 180.0f;
        int inner = r * 42 / 100;
        for (int i = 0; i < 5; ++i) {
            float a = (-90 + 72 * i) * kDeg;
            float b1 = a - 36 * kDeg;
            float b2 = a + 36 * kDeg;
            Triangle(c, cx + (int)(r * cosf(a)), cy + (int)(r * sinf(a)),
                     cx + (int)(inner * cosf(b1)), cy + (int)(inner * sinf(b1)),
                     cx + (int)(inner * cosf(b2)), cy + (int)(inner * sinf(b2)));
        }
        Dot(c, cx, cy, inner);
    }

    static void Drop(const Canvas& c, int cx, int cy, int r) {
        Dot(c, cx, cy, r);
        Triangle(c, cx - r, cy - 1, cx + r, cy - 1, cx, cy - 2 * r - 2);
    }

    static void Z(const Canvas& c, int x, int y, int s) {
        Line(c, x, y, x + s, y, 3);
        Line(c, x + s, y, x, y + s, 3);
        Line(c, x, y + s, x + s, y + s, 3);
    }

    // ---------------------------------------------------------------- face parts
    static void DrawEye(const Canvas& c, Eye eye, int x, bool left, int pdx, int pdy, int pulse) {
        int s = left ? 1 : -1;
        switch (eye) {
        case E_ARC:
            Arc(c, x, kEyeY + 6, 14, 180, 360, 5);
            break;
        case E_ARC_DOWN:
            Arc(c, x, kEyeY - 6, 14, 0, 180, 5);
            break;
        case E_SQUARE:
        case E_ANGRY:
        case E_HALF: {
            uint32_t color = eye == E_ANGRY ? kRed : kBlue;
            Rect(c, x - 16, kEyeY - 15, 32, 30, 8, color);
            Dot(c, x + pdx, kEyeY + pdy, 5, kNavy);
            if (eye != E_SQUARE) {  // slanted lid, inner corner lower
                Line(c, x - 22 * s, kEyeY - 22, x + 22 * s, kEyeY - 6, 9, kNavy);
            }
            break;
        }
        case E_CHEVRON:
            Line(c, x - 11 * s, kEyeY - 11, x + 11 * s, kEyeY, 6);
            Line(c, x + 11 * s, kEyeY, x - 11 * s, kEyeY + 11, 6);
            break;
        case E_LINE:
            Rect(c, x - 15, kEyeY - 3, 30, 7, 4, kBlue);
            break;
        case E_LINE_SHORT:
            Rect(c, x - 10, kEyeY - 3, 20, 6, 3, kBlue);
            break;
        case E_SAD:
            Arc(c, x, kEyeY - 4, 16, 0, 180, 16);
            Line(c, x - 20 * s, kEyeY, x + 20 * s, kEyeY - 9, 7, kNavy);
            break;
        case E_HEART:
            Heart(c, x, kEyeY, 13 + pulse);
            break;
        case E_STAR:
            Star(c, x, kEyeY, 17 + pulse);
            break;
        case E_DOT:
            Dot(c, x, kEyeY, 14);
            Dot(c, x - 5, kEyeY - 5, 3, kNavy);  // shine
            break;
        }
    }

    static void DrawMouth(const Canvas& c, Mouth mouth) {
        switch (mouth) {
        case M_BIG:         Arc(c, kMouthX, 132, 20, 0, 180, 20); break;
        case M_HUGE:        Arc(c, kMouthX, 128, 26, 0, 180, 26); break;
        case M_SMILE:       Arc(c, kMouthX, 114, 32, 35, 145, 4); break;
        case M_SMALL_SMILE: Arc(c, kMouthX, 132, 16, 30, 150, 4); break;
        case M_SMIRK:       Arc(c, kMouthX + 6, 126, 20, 20, 110, 4); break;
        case M_O:           Arc(c, kMouthX, 150, 10, 0, 360, 4); break;
        case M_O_SMALL:     Arc(c, kMouthX, 150, 7, 0, 360, 4); break;
        case M_DOT:         Dot(c, kMouthX, 148, 8); break;
        case M_FLAT:        Line(c, kMouthX - 16, 148, kMouthX + 16, 148, 4); break;
        case M_FLAT_SHORT:  Line(c, kMouthX - 8, 148, kMouthX + 8, 148, 4); break;
        case M_FROWN:       Arc(c, kMouthX, 162, 18, 200, 340, 5); break;
        case M_SMALL_FROWN: Arc(c, kMouthX, 160, 12, 210, 330, 4); break;
        case M_OPEN_SAD:    Arc(c, kMouthX, 160, 14, 180, 360, 14); break;
        case M_WAVY:
            for (int i = 0; i < 4; ++i) {
                int x = kMouthX - 20 + i * 10;
                Line(c, x, 148 + (i % 2 ? -4 : 4), x + 10, 148 + (i % 2 ? 4 : -4), 3);
            }
            break;
        case M_TONGUE:
            Line(c, kMouthX - 20, 136, kMouthX + 20, 136, 4);
            Arc(c, kMouthX, 140, 10, 0, 180, 4);
            Line(c, kMouthX - 8, 136, kMouthX - 8, 141, 4);
            Line(c, kMouthX + 8, 136, kMouthX + 8, 141, 4);
            Line(c, kMouthX, 139, kMouthX, 145, 3);
            break;
        }
    }

    static void DrawExtras(const Canvas& c, uint8_t extras, int z_rise, int tear_fall) {
        if (extras & X_ZZZ) {
            Z(c, 128, 44 - z_rise, 9);
            Z(c, 141, 30 - z_rise, 11);
            Z(c, 156, 16 - z_rise, 13);
        }
        if (extras & X_TEAR) {
            Drop(c, kLeftEyeX - 8, 124 + tear_fall, 5);
            Drop(c, kRightEyeX + 8, 124 + tear_fall, 5);
        }
        if (extras & X_SWEAT) {
            Drop(c, 192, 76, 6);
        }
        if (extras & X_BLUSH) {
            for (int cx : {62, 178}) {
                for (int i = 0; i < 3; ++i) {
                    Line(c, cx - 8 + i * 6, 136, cx - 4 + i * 6, 128, 3);
                }
            }
        }
        if (extras & X_SPARKLE) {
            for (int s : {1, -1}) {
                int x = s > 0 ? 44 : 196;
                Line(c, x, 50, x - 8 * s, 42, 3);
                Line(c, x + 8 * s, 44, x + 8 * s, 32, 3);
                Line(c, x - 2 * s, 58, x - 14 * s, 58, 3);
            }
        }
        if (extras & X_BROWS) {  // worried: inner ends raised
            Line(c, kLeftEyeX - 14, kEyeY - 22, kLeftEyeX + 12, kEyeY - 28, 4);
            Line(c, kRightEyeX + 14, kEyeY - 22, kRightEyeX - 12, kEyeY - 28, 4);
        }
    }

    // Alternate mouth used while speaking: big smiles narrow, everything else opens into an "o".
    static Mouth TalkMouth(Mouth mouth) {
        switch (mouth) {
        case M_BIG:
        case M_HUGE:     return M_SMALL_SMILE;
        case M_OPEN_SAD: return M_SMALL_FROWN;
        case M_O:        return M_O_SMALL;
        default:         return M_O;
        }
    }

    static bool CanBlink(Eye eye) {
        return eye == E_SQUARE || eye == E_DOT || eye == E_HALF || eye == E_HEART || eye == E_STAR;
    }

    void DrawFace(Canvas c) {
        const Mood& m = kMoods[mood_];
        float t = (float)tick_;
        int dx = 0, dy = 0, pulse = 0, look = 0, z_rise = 0, tear_fall = 0;
        switch (m.anim) {
        case A_BOUNCE:  dy = -(int)(fabsf(sinf(t * 0.5f)) * 6.0f); break;
        case A_BREATHE: dy = (int)roundf(sinf(t * 0.2f) * 2.0f); break;
        case A_SHAKE: {
            uint32_t phase = tick_ % 16;
            dx = phase < 6 ? ((phase % 2) ? 4 : -4) : 0;
            break;
        }
        case A_TREMBLE: dx = (tick_ % 2) ? 1 : -1; break;
        case A_PULSE:   pulse = (int)roundf(sinf(t * 0.6f) * 2.5f); break;
        case A_LOOK:    look = (int)roundf(sinf(t * 0.25f) * 6.0f); break;
        case A_FLOATZ:  z_rise = (int)((tick_ % 12) * 1.5f); break;
        case A_TEAR:    tear_fall = (int)((tick_ % 10) * 4); break;
        case A_NONE:    break;
        }
        c.x0 += dx;
        c.y0 += dy;

        // Head outline and antenna stubs
        Rect(c, 32, 62, 176, 116, 30, UINT32_MAX, 4);
        Line(c, 120, 38, 120, 62, 4);
        Line(c, 120, 178, 120, 202, 4);

        int pdx = m.pupil_dx + look;
        int pdy = m.pupil_dy;
        Eye left = (blinking_ && CanBlink(m.eye_l)) ? E_LINE : m.eye_l;
        Eye right = (blinking_ && CanBlink(m.eye_r)) ? E_LINE : m.eye_r;
        DrawEye(c, left, kLeftEyeX, true, pdx, pdy, pulse);
        DrawEye(c, right, kRightEyeX, false, pdx, pdy, pulse);
        Mouth mouth = m.mouth;
        if (speaking_ && (tick_ / 2) % 2 == 0) {  // open/close every 250 ms while talking
            mouth = TalkMouth(mouth);
        }
        DrawMouth(c, mouth);
        DrawExtras(c, m.extras, z_rise, tear_fall);
    }

    // ---------------------------------------------------------------- LVGL callbacks
    static void DrawEventCb(lv_event_t* e) {
        auto* self = static_cast<AuraDisplay*>(lv_event_get_user_data(e));
        lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
        lv_area_t coords;
        lv_obj_get_coords(obj, &coords);
        self->DrawFace({lv_event_get_layer(e), coords.x1, coords.y1});
    }

    // Runs in the LVGL task at 8 fps: advances the animation and blinks every ~3.5 s.
    static void AnimTimerCb(lv_timer_t* timer) {
        auto* self = static_cast<AuraDisplay*>(lv_timer_get_user_data(timer));
        const Mood& m = kMoods[self->mood_];
        self->tick_++;

        bool speaking = Application::GetInstance().GetDeviceState() == kDeviceStateSpeaking;
        bool redraw = m.anim != A_NONE || speaking || speaking != self->speaking_;
        self->speaking_ = speaking;
        if (self->UpdateIdleMood()) {
            redraw = true;
        }
        bool can_blink = CanBlink(m.eye_l) || CanBlink(m.eye_r);
        if (self->blinking_) {
            self->blinking_ = false;
            redraw = true;
        } else if (can_blink && ++self->blink_tick_ >= kBlinkEveryTicks) {
            self->blink_tick_ = 0;
            self->blinking_ = true;
            redraw = true;
        }
        if (redraw) {
            // Only the head area (with room for bounce/shake) is redrawn to save SPI bandwidth.
            lv_area_t coords;
            lv_obj_get_coords(self->face_obj_, &coords);
            lv_area_t area = {coords.x1 + 20, coords.y1 + 4, coords.x1 + 220, coords.y1 + 214};
            lv_obj_invalidate_area(self->face_obj_, &area);
        }
    }

    // Changes the mood from inside the LVGL task (lock already held).
    void ApplyMood(int number) {
        if (mood_ != number - 1) {
            mood_ = number - 1;
            tick_ = 0;
            blinking_ = false;
            lv_obj_invalidate(face_obj_);
            if (on_mood_changed_) {
                on_mood_changed_(number);
            }
        }
    }

    // Greets once when startup finishes and falls asleep after AURA_SLEEP_AFTER_S idle seconds;
    // wakes up as soon as the device leaves the idle state. Returns true if the face changed.
    bool UpdateIdleMood() {
        DeviceState state = Application::GetInstance().GetDeviceState();
        int64_t now = esp_timer_get_time();
        if (state != kDeviceStateIdle) {
            idle_since_us_ = 0;
            if (sleeping_) {
                sleeping_ = false;
                ApplyMood(4);  // Sereno until the conversation sends an emotion
                ESP_LOGI("AuraDisplay", "Face: awake");
                return true;
            }
            return false;
        }
        if (idle_since_us_ == 0) {
            idle_since_us_ = now;
        }
        if (!greeted_) {
            greeted_ = true;
            ApplyMood(1);  // Feliz
            ESP_LOGI("AuraDisplay", "Face: greeting");
            hold_until_us_ = now + 5000 * 1000LL;
            return true;
        }
        if (!sleeping_ && now >= hold_until_us_ && now - idle_since_us_ >= AURA_SLEEP_AFTER_S * 1000000LL) {
            sleeping_ = true;
            ApplyMood(9);  // Dormido
            ESP_LOGI("AuraDisplay", "Face: asleep after %d s idle", AURA_SLEEP_AFTER_S);
            return true;
        }
        return false;
    }

    // Maps the emotions sent by the Xiaozhi server onto moods of the sheet (1-based numbers).
    static int MoodForEmotion(const char* emotion) {
        struct Entry { const char* name; int mood; };
        static const Entry kMap[] = {
            {"neutral", 4},    {"happy", 1},       {"laughing", 83},   {"funny", 10},
            {"sad", 6},        {"angry", 7},       {"crying", 84},     {"loving", 11},
            {"embarrassed", 86}, {"surprised", 2}, {"shocked", 15},    {"thinking", 14},
            {"winking", 16},   {"cool", 52},       {"relaxed", 24},    {"delicious", 5},
            {"kissy", 54},     {"confident", 67},  {"sleepy", 9},      {"silly", 53},
            {"confused", 17},
        };
        for (const auto& entry : kMap) {
            if (strcmp(emotion, entry.name) == 0) {
                return entry.mood;
            }
        }
        return 4;  // status icons (link, microchip_ai, ...) and unknown names: Sereno
    }

    static std::string Lower(const std::string& s) {
        std::string out = s;
        for (auto& ch : out) {
            if (ch >= 'A' && ch <= 'Z') {
                ch = ch - 'A' + 'a';
            }
        }
        return out;
    }

public:
    using SpiLcdDisplay::SpiLcdDisplay;

    // Comma-separated "1 Feliz, 2 Sorprendido, ..." for the MCP tool description.
    static std::string MoodList() {
        std::string list;
        for (int i = 0; i < kMoodCount; ++i) {
            if (i > 0) {
                list += ", ";
            }
            list += std::to_string(i + 1) + " " + kMoods[i].name;
        }
        return list;
    }

    void SetupUI() override {
        bool first_call = !IsSetupUICalled();
        SpiLcdDisplay::SetupUI();
        if (!first_call) {
            return;
        }

        // Dark theme so status texts stay readable over the navy face. SetTheme persists the
        // choice in NVS, so only call it when needed (normally just on the first boot).
        auto* dark = LvglThemeManager::GetInstance().GetTheme("dark");
        if (dark != nullptr && current_theme_ != dark) {
            SetTheme(dark);
        }

        DisplayLockGuard lock(this);
        lv_obj_t* screen = lv_screen_active();

        // The face replaces the emoji in the bottom layer; status bar, notifications and
        // subtitles are created later by the base class and stay on top of it.
        face_obj_ = lv_obj_create(screen);
        lv_obj_remove_style_all(face_obj_);
        lv_obj_set_size(face_obj_, width_, height_);
        lv_obj_set_style_bg_color(face_obj_, lv_color_hex(kNavy), 0);
        lv_obj_set_style_bg_opa(face_obj_, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(face_obj_, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(face_obj_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(face_obj_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_center(face_obj_);
        lv_obj_move_to_index(face_obj_, lv_obj_get_index(emoji_box_));
        lv_obj_add_event_cb(face_obj_, DrawEventCb, LV_EVENT_DRAW_MAIN, this);
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(emoji_label_, "");  // empty in case the base class un-hides emoji_box_
        anim_timer_ = lv_timer_create(AnimTimerCb, kFramePeriodMs, this);

        env_label_ = lv_label_create(lv_layer_top());
        lv_obj_set_style_text_font(env_label_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(env_label_, lv_color_white(), 0);
        lv_obj_set_style_bg_color(env_label_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(env_label_, LV_OPA_50, 0);
        lv_obj_set_style_radius(env_label_, 8, 0);
        lv_obj_set_style_pad_hor(env_label_, 6, 0);
        lv_obj_set_style_pad_ver(env_label_, 1, 0);
        lv_obj_align(env_label_, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_label_set_text(env_label_, "--\xC2\xB0" "C  --%");
    }

    // number: 1..100 as in the mood sheet.
    void SetMood(int number) {
        if (face_obj_ == nullptr || number < 1 || number > kMoodCount) {
            return;
        }
        DisplayLockGuard lock(this);
        ApplyMood(number);
    }

    // Called with the 1-based mood number whenever the face changes (from any task, under the
    // display lock): the callback must only post work, never block.
    void OnMoodChanged(std::function<void(int)> callback) { on_mood_changed_ = std::move(callback); }

    // Accepts a sheet number ("11") or a mood name ("Enamorado", case-insensitive) and keeps it
    // for hold_ms even if the server sends other emotions. Returns the mood number, or 0 if unknown.
    int SetMoodByName(const std::string& name, int hold_ms = 10000) {
        int number = atoi(name.c_str());
        if (number < 1 || number > kMoodCount) {
            number = 0;
            std::string wanted = Lower(name);
            for (int i = 0; i < kMoodCount; ++i) {
                if (Lower(kMoods[i].name) == wanted) {
                    number = i + 1;
                    break;
                }
            }
        }
        if (number != 0) {
            SetMood(number);
            DisplayLockGuard lock(this);
            int64_t now = esp_timer_get_time();
            hold_until_us_ = now + (int64_t)hold_ms * 1000;
            sleeping_ = false;     // wake up and restart the sleep countdown
            idle_since_us_ = now;
        }
        return number;
    }

    static const char* MoodName(int number) {
        return (number >= 1 && number <= kMoodCount) ? kMoods[number - 1].name : "";
    }

    void SetEmotion(const char* emotion) override {
        if (face_obj_ == nullptr) {
            SpiLcdDisplay::SetEmotion(emotion);
            return;
        }
        if (esp_timer_get_time() < hold_until_us_ || sleeping_) {
            return;  // a requested/greeting face is on hold, or it is asleep until something happens
        }
        SetMood(MoodForEmotion(emotion));
    }

    // alert: 0 normal, 1 too hot (red text), -1 too cold (light blue text).
    void SetEnvironment(int temperature, int humidity, bool valid, int alert = 0) {
        if (env_label_ == nullptr) {
            return;
        }
        char text[32];
        if (valid) {
            snprintf(text, sizeof(text), "%d\xC2\xB0" "C  %d%%", temperature, humidity);
        } else {
            snprintf(text, sizeof(text), "--\xC2\xB0" "C  --%%");
        }
        DisplayLockGuard lock(this);
        uint32_t color = alert > 0 ? 0xFF5A4F : (alert < 0 ? 0x7FD4FF : 0xFFFFFF);
        lv_obj_set_style_text_color(env_label_, lv_color_hex(color), 0);
        lv_label_set_text(env_label_, text);
    }
};

#endif // __AURA_DISPLAY_H__
