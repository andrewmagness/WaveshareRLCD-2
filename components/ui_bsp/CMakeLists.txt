# Only compile custom dashboard files, exclude GUI Guider demo screen
file(GLOB_RECURSE custom_src ./custom/*.c)

# Only include needed fonts (18, 20, 25px) - excluding the 2.5MB 100px font
set(font_src
  ${CMAKE_CURRENT_SOURCE_DIR}/generated/guider_fonts/lv_font_MISANSMEDIUM_18.c
  ${CMAKE_CURRENT_SOURCE_DIR}/generated/guider_fonts/lv_font_MISANSMEDIUM_20.c
  ${CMAKE_CURRENT_SOURCE_DIR}/generated/guider_fonts/lv_font_MISANSMEDIUM_25.c
  ${CMAKE_CURRENT_SOURCE_DIR}/generated/guider_fonts/lv_font_montserratMedium_16.c
)

idf_component_register(
  SRCS ${custom_src} ${font_src}
  REQUIRES lvgl user_app
  INCLUDE_DIRS "custom" "generated" "generated/guider_customer_fonts")

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)


