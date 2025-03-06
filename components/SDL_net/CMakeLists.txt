idf_component_register(
    SRCS "SDLnet.c" "SDLnetselect.c" "SDLnetTCP.c" "SDLnetUDP.c"
    INCLUDE_DIRS "." "../OpenTyrian" "../SDL"
    REQUIRES driver fatfs esp_timer esp_wifi
)
