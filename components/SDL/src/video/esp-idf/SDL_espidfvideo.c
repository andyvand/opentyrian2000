
#include "SDL_internal.h"
#include "SDL_espidfvideo.h"
#include "SDL_espidfshared.h"
#include "SDL_espidfframebuffer.h"
#include "SDL_espidfevents.h"
#include "SDL_espidftouch.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "esp_log.h"

#ifdef SDL_VIDEO_DRIVER_PRIVATE

#include "video/SDL_sysvideo.h"
#include "video/SDL_pixels_c.h"
#include "events/SDL_events_c.h"

#define ESPIDFVID_DRIVER_NAME "espidf"

esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t panel_io_handle = NULL;

static bool ESPIDF_VideoInit(SDL_VideoDevice *_this);
static void ESPIDF_VideoQuit(SDL_VideoDevice *_this);

static bool ESPIDF_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_MOVED, window->floating.x, window->floating.y);
    return true;
}

static void ESPIDF_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_SendWindowEvent(window, SDL_EVENT_WINDOW_RESIZED, window->floating.w, window->floating.h);
}

static SDL_VideoDevice *ESPIDF_CreateDevice(void)
{
    SDL_VideoDevice *device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) return NULL;

    device->VideoInit = ESPIDF_VideoInit;
    device->VideoQuit = ESPIDF_VideoQuit;
    device->SetWindowPosition = ESPIDF_SetWindowPosition;
    device->SetWindowSize = ESPIDF_SetWindowSize;
    device->PumpEvents = ESPIDF_PumpEvents;
    device->CreateWindowFramebuffer = SDL_ESPIDF_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_ESPIDF_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_ESPIDF_DestroyWindowFramebuffer;

    return device;
}

VideoBootStrap PRIVATE_bootstrap = {
    ESPIDFVID_DRIVER_NAME, "SDL esp-idf video driver",
    ESPIDF_CreateDevice, NULL, false
};

static bool ESPIDF_VideoInit(SDL_VideoDevice *_this)
{
    SDL_DisplayMode mode;
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGB565;
    mode.w = BSP_LCD_H_RES;
    mode.h = BSP_LCD_V_RES;
    printf("ESP-IDF video init\n");
    if (SDL_AddBasicVideoDisplay(&mode) == 0) {
        return false;
    }

#ifdef CONFIG_IDF_TARGET_ESP32P4
    const bsp_display_config_t bsp_disp_cfg = {
        .dsi_bus = {
            .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
            .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
        }
    };
#else
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = (BSP_LCD_H_RES * BSP_LCD_V_RES) * sizeof(uint16_t),
    };
#endif

    ESP_ERROR_CHECK(bsp_display_new(&bsp_disp_cfg, &panel_handle, &panel_io_handle));

    ESP_ERROR_CHECK(bsp_display_backlight_on());

#ifndef CONFIG_IDF_TARGET_ESP32P4
    esp_lcd_panel_disp_on_off(panel_handle, true);
#endif

    ESPIDF_InitTouch();
    return true;
}

static void ESPIDF_VideoQuit(SDL_VideoDevice *_this)
{
    // Clean up BSP resources if needed
}

#endif /* SDL_VIDEO_DRIVER_PRIVATE */
