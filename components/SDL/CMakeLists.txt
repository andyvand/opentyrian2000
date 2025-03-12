idf_component_register(
    SRCS "SDL_system.c" "SDL_video.c" "SDL_input.c" "spi_lcd.c" "SDL_event.c" "SDL_audio.c" "SDL_error.c" "spi_lcd.c"
    INCLUDE_DIRS "." "../OpenTyrian" "../esp_lcd_touch_xpt2046" "../esp_lcd_touch" "../esp_lcd_qemu_rgb/interface"
    REQUIRES driver fatfs esp_adc esp_timer esp_lcd esp_lcd_touch esp_lcd_touch_xpt2046 esp_littlefs esp_lcd_qemu_rgb
)
