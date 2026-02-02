idf_component_register(
    SRCS "user_app.cpp" "sports_scores.c" "logo_fetcher.c"
    PRIV_REQUIRES ui_bsp app_bsp port_bsp esp_http_client json esp-tls esp_netif esp_adc esp_driver_i2c lvgl
    INCLUDE_DIRS "./")

# Use custom SPIRAM allocators for LodePNG (defined in logo_fetcher.c)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LODEPNG_NO_COMPILE_ALLOCATORS)


