# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

# AURA-902: plain `idf.py` builds start from the board's defaults (Spanish, "Hi ESP" wake word).
# scripts/build.py passes -DSDKCONFIG_DEFAULTS itself, so this does not affect it.
if(NOT DEFINED SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;main/boards/waveshare/esp32-p4-nano/sdkconfig.aura902")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

set(PROJECT_VER "2.5.3")
project(xiaozhi)
