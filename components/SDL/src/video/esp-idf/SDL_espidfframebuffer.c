#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PRIVATE

#include "video/SDL_sysvideo.h"
#include "SDL_properties_c.h"
#include "SDL_espidfframebuffer.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "SDL_espidfshared.h"
#include "esp_heap_caps.h"
#include "freertos/semphr.h"
#ifdef CONFIG_IDF_TARGET_ESP32P4
#include "driver/ppa.h"
#endif

static const char *TAG = "SDL_espidfframebuffer";

#define ESPIDF_SURFACE "SDL.internal.window.surface"

static SemaphoreHandle_t lcd_semaphore;
static int max_chunk_height = 4;  // Configurable chunk height
#ifdef CONFIG_IDF_TARGET_ESP32P4
static ppa_client_handle_t ppa_srm_handle = NULL;  // PPA client handle
static uint8_t *ppa_out_buf = NULL;  // Reusable PPA output buffer
static size_t ppa_out_buf_size = 0;  // Size of the PPA output buffer

#ifndef SCALE_FACTOR
int scale_factor = 1;
float scale_factor_float = 1.0;
// Workaround to quickly pass scaling to PPA
// This should be probably handled on Render level
void set_scale_factor(int factor, float factor_float) {
    scale_factor = factor;
    scale_factor_float = factor_float;
}
#endif

#else
static uint16_t *rgb565_buffer = NULL;
#endif

#ifdef CONFIG_IDF_TARGET_ESP32P4
static bool lcd_event_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    xSemaphoreGive(lcd_semaphore);
    return false;
}
#else
static bool lcd_event_callback(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event_data, void *user_ctx)
{
    xSemaphoreGive(lcd_semaphore);
    return false;
}
#endif

void esp_idf_log_free_dma(void) {
    size_t free_dma = heap_caps_get_free_size(MALLOC_CAP_DMA);
    ESP_LOGI(TAG, "Free DMA memory: %d bytes", free_dma);
}

bool SDL_ESPIDF_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, SDL_PixelFormat *format, void **pixels, int *pitch)
{
    SDL_Surface *surface;
    int w, h;

    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGB565);
    if (!surface) {
        return false;
    }

    SDL_SetSurfaceProperty(SDL_GetWindowProperties(window), ESPIDF_SURFACE, surface);
    *format = SDL_PIXELFORMAT_RGB565;
    *pixels = surface->pixels;
    *pitch = surface->pitch;

#ifndef CONFIG_IDF_TARGET_ESP32P4
    // Allocate RGB565 buffer in IRAM
    rgb565_buffer = heap_caps_malloc(w * max_chunk_height * sizeof(uint16_t), MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
    if (!rgb565_buffer) {
        SDL_DestroySurface(surface);
        return SDL_SetError("Failed to allocate memory for RGB565 buffer");
    }
#endif

    // Create a semaphore to synchronize LCD transactions
    lcd_semaphore = xSemaphoreCreateBinary();
    if (!lcd_semaphore) {
        SDL_DestroySurface(surface);
        return SDL_SetError("Failed to create semaphore");
    }

    // Initialize PPA (only for ESP32-P4)
#ifdef CONFIG_IDF_TARGET_ESP32P4
    if (!ppa_srm_handle) {
        ppa_client_config_t ppa_srm_config = {
            .oper_type = PPA_OPERATION_SRM,
            .max_pending_trans_num = 1,
        };
        ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &ppa_srm_handle));
    }

    if (scale_factor != 1) {
        // Allocate reusable PPA output buffer
        ppa_out_buf_size = (w * scale_factor) * (max_chunk_height * scale_factor) * sizeof(uint16_t);  // 2x scaling
        ppa_out_buf = heap_caps_malloc(ppa_out_buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (!ppa_out_buf) {
            return SDL_SetError("Failed to allocate PPA output buffer");
        }
    }
    const esp_lcd_dpi_panel_event_callbacks_t callback = {
        .on_color_trans_done = lcd_event_callback,
    };
    esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &callback, NULL);
#else
#if !CONFIG_BSP_DISPLAY_DRIVER_QEMU
    esp_lcd_panel_io_register_event_callbacks(panel_io_handle, &(esp_lcd_panel_io_callbacks_t){ .on_color_trans_done = lcd_event_callback }, NULL);
#endif
#endif

    return true;
}

IRAM_ATTR bool SDL_ESPIDF_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_Surface *surface = (SDL_Surface *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), ESPIDF_SURFACE, NULL);
    if (!surface) {
        return SDL_SetError("Couldn't find ESPIDF surface for window");
    }

#ifdef CONFIG_IDF_TARGET_ESP32P4
    // Iterate over the framebuffer in chunks
    for (int y = 0; y < surface->h; y += max_chunk_height) {
        int height = (y + max_chunk_height > surface->h) ? (surface->h - y) : max_chunk_height;
        uint16_t *src_pixels = (uint16_t *)surface->pixels + (y * surface->w);

        if (scale_factor != 1) {
            // PPA SRM configuration for scaling
            ppa_srm_oper_config_t srm_config = {
                .in.buffer = src_pixels,
                .in.pic_w = surface->w,
                .in.pic_h = height,
                .in.block_w = surface->w,
                .in.block_h = height,
                .in.srm_cm = PPA_SRM_COLOR_MODE_RGB565,

                .out.srm_cm = PPA_SRM_COLOR_MODE_RGB565,
                .out.buffer = ppa_out_buf,
                .out.buffer_size = ppa_out_buf_size,  // Reused output buffer
                .out.pic_w = surface->w * scale_factor,
                .out.pic_h = height * scale_factor,

                .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,  // No rotation
                .scale_x = scale_factor_float,
                .scale_y = scale_factor_float,

                .rgb_swap = 0,
                .byte_swap = 0,
                .mode = PPA_TRANS_MODE_BLOCKING,
            };

            // Execute PPA scaling
            ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa_srm_handle, &srm_config));

            // Draw the scaled output to the LCD
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y * scale_factor, surface->w * scale_factor, (y + height) * scale_factor, ppa_out_buf));
        } else {
            // Draw the scaled output to the LCD
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, surface->w, (y + height), src_pixels));
        }

        // Wait for the current chunk to finish transmission
        xSemaphoreTake(lcd_semaphore, portMAX_DELAY);
    }
#else
    // Without PPA, send chunks directly from src_pixels
    for (int y = 0; y < surface->h; y += max_chunk_height) {
        int height = (y + max_chunk_height > surface->h) ? (surface->h - y) : max_chunk_height;

        for (int i = 0; i < surface->w * max_chunk_height; i++) {
            uint16_t rgba = ((uint16_t *)surface->pixels)[y * surface->w + i];
            uint8_t g = (rgba >> 11) & 0xFF;
            uint8_t r = (rgba >> 5) & 0xFF;
            uint8_t b = (rgba >> 0) & 0xFF;

            rgb565_buffer[i] = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
        }
        // Send directly to LCD
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, surface->w, y + height, rgb565_buffer));

        // Wait for the current chunk to finish transmission
        xSemaphoreTake(lcd_semaphore, portMAX_DELAY);
    }
#endif

    return true;
}

void SDL_ESPIDF_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_ClearProperty(SDL_GetWindowProperties(window), ESPIDF_SURFACE);

    // Delete the semaphore
    if (lcd_semaphore) {
        vSemaphoreDelete(lcd_semaphore);
        lcd_semaphore = NULL;
    }

#ifdef CONFIG_IDF_TARGET_ESP32P4
    // Free PPA output buffer
    if (ppa_out_buf) {
        heap_caps_free(ppa_out_buf);
        ppa_out_buf = NULL;
        ppa_out_buf_size = 0;
    }

    if (ppa_srm_handle) {
        ESP_ERROR_CHECK(ppa_unregister_client(ppa_srm_handle));
        ppa_srm_handle = NULL;
    }
#else
    // Free the RGB565 buffer
    if (rgb565_buffer) {
        heap_caps_free(rgb565_buffer);
        rgb565_buffer = NULL;
    }
#endif
}

#endif /* SDL_VIDEO_DRIVER_PRIVATE */
