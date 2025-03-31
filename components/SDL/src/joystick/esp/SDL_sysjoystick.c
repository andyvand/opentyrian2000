/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_PRIVATE

// This is the ESP implementation of the SDL joystick API

#include "../../SDL/src/joystick/SDL_sysjoystick.h"

#if CONFIG_HW_ODROID_GO
#define NB_BUTTONS 6
#define NB_AXIS 2
#else
#define NB_BUTTONS 10
#define NB_AXIS 0
#endif

#define VERBOSE 1

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define ODROID_GAMEPAD_IO_X ADC_CHANNEL_6
#define ODROID_GAMEPAD_IO_Y ADC_CHANNEL_7

static QueueHandle_t gpio_evt_queue = NULL;

/*
  ESP sticks values are roughly within +/-160
  which is too small to pass the jitter tolerance.
  This correction is applied to axis values
  so they fit better in SDL's value range.
*/
static inline int Correct_Axis_X(int X) {
    if (X > 2048 + 1024) {
        return -SDL_JOYSTICK_AXIS_MAX;
    }
    else if (X > 1024) {
        return SDL_JOYSTICK_AXIS_MAX;
    }
    return 0;
}

static inline int Correct_Axis_Y(int X) {
    if (X > 2048 + 1024) {
        return SDL_JOYSTICK_AXIS_MAX;
    }
    else if (X > 1024) {
        return -SDL_JOYSTICK_AXIS_MAX;
    }
    return 0;
}

#if CONFIG_HW_ODROID_GO
#define CONFIG_HW_BUTTON_PIN_NUM_BUTTON1 32
#define CONFIG_HW_BUTTON_PIN_NUM_BUTTON2 33
#define CONFIG_HW_BUTTON_PIN_NUM_VOL 0
#define CONFIG_HW_BUTTON_PIN_NUM_MENU 13
#define CONFIG_HW_BUTTON_PIN_NUM_START 39
#define CONFIG_HW_BUTTON_PIN_NUM_SELECT 27
#else
#define CONFIG_HW_BUTTON_PIN_NUM_UP CONFIG_BSP_BUTTON_1_GPIO
#define CONFIG_HW_BUTTON_PIN_NUM_DOWN CONFIG_BSP_BUTTON_2_GPIO
#define CONFIG_HW_BUTTON_PIN_NUM_LEFT CONFIG_BSP_BUTTON_3_GPIO
#define CONFIG_HW_BUTTON_PIN_NUM_RIGHT CONFIG_BSP_BUTTON_4_GPIO
#define CONFIG_HW_BUTTON_PIN_NUM_BUTTON1 CONFIG_BSP_BUTTON_5_GPIO
#define CONFIG_HW_BUTTON_PIN_NUM_BUTTON2 CONFIG_BSP_BUTTON_6_GPIO
#define CONFIG_HW_BUTTON_PIN_NUM_MENU CONFIG_BSP_BUTTON_7_GPIO
#define CONFIG_HW_BUTTON_PIN_NUM_START CONFIG_BSP_BUTTON_8_GPIO
#define CONFIG_HW_BUTTON_PIN_NUM_SELECT CONFIG_BSP_BUTTON_9_GPIO
#define CONFIG_HW_BUTTON_PIN_NUM_VOL CONFIG_BSP_BUTTON_10_GPIO
#endif

const int button_gpio[NB_BUTTONS] = {
#if CONFIG_HW_ODROID_GO
    CONFIG_HW_BUTTON_PIN_NUM_BUTTON1,
    CONFIG_HW_BUTTON_PIN_NUM_BUTTON2,
    CONFIG_HW_BUTTON_PIN_NUM_VOL,
    CONFIG_HW_BUTTON_PIN_NUM_MENU,
    CONFIG_HW_BUTTON_PIN_NUM_START,
    CONFIG_HW_BUTTON_PIN_NUM_SELECT
#else
    CONFIG_HW_BUTTON_PIN_NUM_UP,
    CONFIG_HW_BUTTON_PIN_NUM_DOWN,
    CONFIG_HW_BUTTON_PIN_NUM_LEFT,
    CONFIG_HW_BUTTON_PIN_NUM_RIGHT,
    CONFIG_HW_BUTTON_PIN_NUM_BUTTON1,
    CONFIG_HW_BUTTON_PIN_NUM_BUTTON2,
    CONFIG_HW_BUTTON_PIN_NUM_START,
    CONFIG_HW_BUTTON_PIN_NUM_SELECT,
    CONFIG_HW_BUTTON_PIN_NUM_MENU,
    CONFIG_HW_BUTTON_PIN_NUM_VOL
#endif
};

#if CONFIG_HW_ODROID_GO
adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
};
adc_oneshot_chan_cfg_t config = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
};
#endif

static void UpdateESPPressedButtons(Uint64 timestamp, SDL_Joystick *joystick);
static void UpdateESPReleasedButtons(Uint64 timestamp, SDL_Joystick *joystick);
static void UpdateESPAxes(Uint64 timestamp, SDL_Joystick *joystick);

#ifndef CONFIG_HW_ODROID_GO
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    int gpio_num = (int)arg;
    for (int i=0; i < 6; i++)
    {
        if(button_gpio[i] == gpio_num)
        {
            xQueueSendFromISR(gpio_evt_queue, &(button_gpio[i]), NULL);
        }
    }
}
#endif

static bool ESP_JoystickInit(void)
{
    gpio_config_t io_conf;
    io_conf.pull_down_en = 0;

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;

    //bit mask of the pins, use GPIO... here
    for (int i=0; i < NB_BUTTONS; i++)
    {
        if(i==0)
        {
            io_conf.pin_bit_mask = (1ULL<<button_gpio[i]);
        } else {
            io_conf.pin_bit_mask |= (1ULL<<button_gpio[i]);
        }
    }

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
#if CONFIG_HW_ODROID_GO
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ODROID_GAMEPAD_IO_X, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ODROID_GAMEPAD_IO_Y, &config));
#else
    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(int));
    //start gpio task
    //xTaskCreatePinnedToCore(&gpioTask, "GPIO", 1500, NULL, 7, NULL, 0);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_SHARED);

    //hook isr handler
    for (int i=0; i < 6; i++)
    {
        gpio_isr_handler_add(button_gpio[i], gpio_isr_handler, (void *)button_gpio[i]);
    }
#endif

    SDL_PrivateJoystickAdded(1);

    return true;
}

static const char *ESP_JoystickGetDeviceName(int device_index)
{
    return "ESP Joystick";
}

static int ESP_JoystickGetCount(void)
{
    return 1;
}

static SDL_GUID ESP_JoystickGetDeviceGUID(int device_index)
{
    SDL_GUID guid = SDL_CreateJoystickGUIDForName("ESP Joystick");
    return guid;
}

static SDL_JoystickID ESP_JoystickGetDeviceInstanceID(int device_index)
{
    return device_index + 1;
}

static bool ESP_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    joystick->nbuttons = NB_BUTTONS;
    joystick->naxes = NB_AXIS;
    joystick->nhats = 0;

    return true;
}

static bool ESP_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

static void UpdateESPPressedButtons(Uint64 timestamp, SDL_Joystick *joystick)
{
#if CONFIG_HW_ODRIOID_GO
    for (Uint8 i = 0; i < joystick->nbuttons; i++) {
#else
    for (Uint8 i = 4; i < joystick->nbuttons; i++) {
#endif
        if (1-gpio_get_level(button_gpio[i])) {
#ifdef VERBOSE
            ESP_LOGI("SDL", "Button %d pressed\n", i);
#endif
            SDL_SendJoystickButton(timestamp, joystick, i, true);
        }
    }
}

static void UpdateESPReleasedButtons(Uint64 timestamp, SDL_Joystick *joystick)
{
#if CONFIG_HW_ODRIOID_GO
    for (Uint8 i = 0; i < joystick->nbuttons; i++) {
#else
    for (Uint8 i = 4; i < joystick->nbuttons; i++) {
#endif
        if (!(1-gpio_get_level(button_gpio[i]))) {
            SDL_SendJoystickButton(timestamp, joystick, i, false);
        }
    }
}

typedef struct Axes_Val
{
    int dx;
    int dy;
} Axes_Val_t;

static void UpdateESPAxes(Uint64 timestamp, SDL_Joystick *joystick)
{
#if CONFIG_HW_ODROID_GO
    static Axes_Val_t previous_state = { 0, 0 };
    Axes_Val_t current_state;

    if (joystick->naxes != 0)
    {
        adc_oneshot_read(adc1_handle, ODROID_GAMEPAD_IO_X, &current_state.dx);
        if (previous_state.dx != current_state.dx) {
            SDL_SendJoystickAxis(timestamp, joystick,
                                 2,
                                 Correct_Axis_X(current_state.dx));
        }
        adc_oneshot_read(adc1_handle, ODROID_GAMEPAD_IO_Y, &current_state.dy);
        if (previous_state.dy != current_state.dy) {
            SDL_SendJoystickAxis(timestamp, joystick,
                                 3,
                                 Correct_Axis_Y(current_state.dy));
        }
        previous_state = current_state;
    }
#else
    if (1-gpio_get_level(button_gpio[0]))
    {
#ifdef VERBOSE
        ESP_LOGI("SDL", "Up pressed\n");
#endif
        SDL_SendJoystickAxis(timestamp, joystick,
                             3,
                             SDL_JOYSTICK_AXIS_MAX);
    } else if (1-gpio_get_level(button_gpio[1])) {
#ifdef VERBOSE
        ESP_LOGI("SDL", "Down pressed\n");
#endif
        SDL_SendJoystickAxis(timestamp, joystick,
                             3,
                             -SDL_JOYSTICK_AXIS_MAX);
    } else {
        SDL_SendJoystickAxis(timestamp, joystick,
                             3,
                             0);
    }
            
    if (1-gpio_get_level(button_gpio[2]))
    {
#ifdef VERBOSE
        ESP_LOGI("SDL", "Left pressed\n");
#endif
        SDL_SendJoystickAxis(timestamp, joystick,
                             2,
                             -SDL_JOYSTICK_AXIS_MAX);
    } else if (1-gpio_get_level(button_gpio[3])) {
#ifdef VERBOSE
        ESP_LOGI("SDL", "Right pressed\n");
#endif
        SDL_SendJoystickAxis(timestamp, joystick,
                             2,
                             SDL_JOYSTICK_AXIS_MAX);
    } else {
        SDL_SendJoystickAxis(timestamp, joystick,
                             2,
                             0);
    }
#endif
}

static void ESP_JoystickUpdate(SDL_Joystick *joystick)
{
    Uint64 timestamp = SDL_GetTicksNS();

    UpdateESPPressedButtons(timestamp, joystick);
    UpdateESPReleasedButtons(timestamp, joystick);
    UpdateESPAxes(timestamp, joystick);
}

static void ESP_JoystickClose(SDL_Joystick *joystick)
{
}

static void ESP_JoystickQuit(void)
{
}

static bool ESP_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    // There is only one possible mapping.
    *out = (SDL_GamepadMapping){
#if CONFIG_HW_ODROID_GO
        .a = { EMappingKind_Button, 0 },
        .b = { EMappingKind_Button, 1 },
        .x = { EMappingKind_Button, 2 },
        .y = { EMappingKind_Button, 5 },
        .back = { EMappingKind_Button, 3 },
        .start = { EMappingKind_Button, 4 },
#else
        .a = { EMappingKind_Button, 4 },
        .b = { EMappingKind_Button, 5 },
        .x = { EMappingKind_Button, 7 },
        .y = { EMappingKind_Button, 9 },
        .back = { EMappingKind_Button, 8 },
        .start = { EMappingKind_Button, 6 },
#endif
        .guide = { EMappingKind_None, 255 },
        .leftstick = { EMappingKind_None, 255 },
        .rightstick = { EMappingKind_None, 255 },
        .leftshoulder = { EMappingKind_None, 255 },
        .rightshoulder = { EMappingKind_None, 255 },
        .dpup = { EMappingKind_None, 255 },
        .dpdown = { EMappingKind_None, 255 },
        .dpleft = { EMappingKind_None, 255 },
        .dpright = { EMappingKind_None, 255 },
        .misc1 = { EMappingKind_None, 255 },
        .right_paddle1 = { EMappingKind_None, 255 },
        .left_paddle1 = { EMappingKind_None, 255 },
        .right_paddle2 = { EMappingKind_None, 255 },
        .left_paddle2 = { EMappingKind_None, 255 },
        .leftx = { EMappingKind_None, 255 },
        .lefty = { EMappingKind_None, 255 },
        .rightx = { EMappingKind_Axis, 2 },
        .righty = { EMappingKind_Axis, 3 },
        .lefttrigger = { EMappingKind_None, 255 },
        .righttrigger = { EMappingKind_None, 255 },
    };
    return true;
}

static void ESP_JoystickDetect(void)
{
}

static bool ESP_JoystickIsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
    // We don't override any other drivers
    return false;
}

static const char *ESP_JoystickGetDevicePath(int device_index)
{
    return NULL;
}

static int ESP_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index)
{
    return -1;
}

static int ESP_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
}

static void ESP_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static bool ESP_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static bool ESP_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static bool ESP_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool ESP_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

SDL_JoystickDriver SDL_PRIVATE_JoystickDriver = {
    ESP_JoystickInit,
    ESP_JoystickGetCount,
    ESP_JoystickDetect,
    ESP_JoystickIsDevicePresent,
    ESP_JoystickGetDeviceName,
    ESP_JoystickGetDevicePath,
    ESP_JoystickGetDeviceSteamVirtualGamepadSlot,
    ESP_JoystickGetDevicePlayerIndex,
    ESP_JoystickSetDevicePlayerIndex,
    ESP_JoystickGetDeviceGUID,
    ESP_JoystickGetDeviceInstanceID,
    ESP_JoystickOpen,
    ESP_JoystickRumble,
    ESP_JoystickRumbleTriggers,
    ESP_JoystickSetLED,
    ESP_JoystickSendEffect,
    ESP_JoystickSetSensorsEnabled,
    ESP_JoystickUpdate,
    ESP_JoystickClose,
    ESP_JoystickQuit,
    ESP_JoystickGetGamepadMapping
};
#endif
