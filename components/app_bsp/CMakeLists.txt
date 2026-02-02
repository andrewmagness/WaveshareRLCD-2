idf_component_register(
    SRCS
    "lvgl_bsp.cpp"
    "esp_wifi_bsp.c"
    "sntp_bsp.c"
    PRIV_REQUIRES
    esp_wifi
    esp_event
    nvs_flash
    ui_bsp
    port_bsp
    esp_timer
    REQUIRES
    lvgl__lvgl
    INCLUDE_DIRS 
    "./")

