idf_component_register(
    SRCS "esp_lcd_touch.c"
    INCLUDE_DIRS "include"
    REQUIRES driver fatfs esp_timer esp_lcd
)
