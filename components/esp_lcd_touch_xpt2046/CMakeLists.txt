idf_component_register(
    SRCS "esp_lcd_touch_xpt2046.c"
    INCLUDE_DIRS "include" "../esp_lcd_touch/include"
    REQUIRES driver fatfs esp_timer esp_lcd_touch
)
