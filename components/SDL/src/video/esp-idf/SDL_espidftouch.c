#include "events/SDL_touch_c.h"
#include "events/SDL_mouse_c.h"
#include "video/SDL_sysvideo.h"
#include "SDL_espidftouch.h"
#include <stdbool.h>

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#if BSP_CAPS_TOUCH == 1
#include "bsp/touch.h"
esp_lcd_touch_handle_t touch_handle;   // LCD touch handle
#endif
#include "esp_log.h"

//#define ESPIDF_TOUCH_ID       1
#define ESPIDF_TOUCH_FINGER     1


void ESPIDF_InitTouch(void)
{
#if CONFIG_BSP_TOUCH_ENABLED
    bsp_i2c_init();

    /* Initialize touch */
    bsp_touch_new(NULL, &touch_handle);

    SDL_AddTouch(SDL_MOUSE_TOUCHID, SDL_TOUCH_DEVICE_DIRECT, "mouse_input");
    ESP_LOGI("SDL", "ESPIDF_InitTouch");
#endif
}

void ESPIDF_PumpTouchEvent(void)
{
#if CONFIG_BSP_TOUCH_ENABLED
    SDL_Window *window;
    SDL_VideoDisplay *display;
    static bool was_pressed = false;
    bool pressed;

    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    esp_lcd_touch_read_data(touch_handle);
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(touch_handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);
    pressed = (touchpad_x[0] != 0 || touchpad_y[0] != 0);

    display = NULL;
    window = display ? display->fullscreen_window : NULL;

    if (pressed != was_pressed) {
        was_pressed = pressed;
        ESP_LOGI("SDL", "touchpad_pressed: %d, [%d, %d]", touchpad_pressed, touchpad_x[0], touchpad_y[0]);
#if 1
        if (pressed)
        {
            SDL_PerformWarpMouseInWindow(window, touchpad_x[0], touchpad_y[0], false);
        }
        SDL_SendMouseButton(0, window, SDL_MOUSE_TOUCHID, 1, pressed);
#else
        SDL_SendTouch(0, SDL_MOUSE_TOUCHID, ESPIDF_TOUCH_FINGER,
                      window,
                      pressed,
                      touchpad_x[0],
                      touchpad_y[0],
                      pressed ? 1.0f : 0.0f);
#endif
    } else if (pressed) {
#if 0
        SDL_SendTouchMotion(0, SDL_MOUSE_TOUCHID, ESPIDF_TOUCH_FINGER,
                            window,
                            touchpad_x[0],
                            touchpad_y[0],
                            1.0f);
#else
        SDL_PerformWarpMouseInWindow(window, touchpad_x[0], touchpad_y[0], false);
        SDL_SendMouseButton(0, window, SDL_MOUSE_TOUCHID, 1, pressed);
#endif
    }
#endif
}

int ESPIDF_CalibrateTouch(float screenX[], float screenY[], float touchX[], float touchY[])
{
    return 0;
}

void ESPIDF_ChangeTouchMode(int raw)
{
    return;
}

void ESPIDF_ReadTouchRawPosition(float* x, float* y)
{
    return;
}

void ESPIDF_QuitTouch(void)
{
    // ts_close(ts);
}
