# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS components/ExternLib)

# Use custom SPIRAM allocators for LodePNG (defined in logo_fetcher.c)
# This must be added before project() to affect LVGL's lodepng.c
add_compile_definitions(LODEPNG_NO_COMPILE_ALLOCATORS)

project(WaveshareRLCD-2)

