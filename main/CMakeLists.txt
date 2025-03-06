set(CMAKE_C_FLAGS "-DWITH_SDL ${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "-DWITH_NETWORK ${CMAKE_C_FLAGS}")

idf_component_register(SRCS "app_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES OpenTyrian SDL SDL_net esp_lcd esp_wifi)
