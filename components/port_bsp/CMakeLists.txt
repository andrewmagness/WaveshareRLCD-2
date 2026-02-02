idf_component_register(
    SRCS 
    "i2c_bsp.cpp" 
    "display_bsp.cpp" 
    "sdcard_bsp.cpp"
    "./src/multi_button/multi_button.c" 
    "button_bsp.c"
    "./i2c_equipment.cpp"
    "./adc_bsp.cpp"
    PRIV_REQUIRES 
    driver
    SensorLib
    esp_timer
    esp_adc
    REQUIRES
    esp_lcd
    esp_driver_sdmmc
    fatfs
    INCLUDE_DIRS 
    "./" 
    "./src/multi_button")



