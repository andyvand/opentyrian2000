/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_spiffs.h"
#include "esp_littlefs.h"
#include "esp_vfs_fat.h"

#include "bsp/esp_bsp_generic.h"
#include "bsp_err_check.h"
#include "button_gpio.h"
#include "button_adc.h"
#include "driver/i2s_std.h"

#if CONFIG_BSP_DISPLAY_ENABLED
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "bsp/display.h"

#if CONFIG_BSP_DISPLAY_DRIVER_ILI9341
#include "esp_lcd_ili9341.h"
#elif CONFIG_BSP_DISPLAY_DRIVER_GC9A01
#include "esp_lcd_gc9a01.h"
#elif CONFIG_BSP_DISPLAY_DRIVER_QEMU
#include "esp_lcd_qemu_rgb.h"
#endif

#endif

#if CONFIG_BSP_TOUCH_ENABLED
#include "bsp/touch.h"
#if CONFIG_BSP_TOUCH_DRIVER_TT21100
#include "esp_lcd_touch_tt21100.h"
#elif CONFIG_BSP_TOUCH_DRIVER_GT1151
#include "esp_lcd_touch_gt1151.h"
#elif CONFIG_BSP_TOUCH_DRIVER_GT911
#include "esp_lcd_touch_gt911.h"
#elif CONFIG_BSP_TOUCH_DRIVER_CST816S
#include "esp_lcd_touch_cst816s.h"
#elif CONFIG_BSP_TOUCH_DRIVER_FT5X06
#include "esp_lcd_touch_ft5x06.h"
#elif CONFIG_BSP_TOUCH_DRIVER_XPT2046
#include "esp_lcd_touch_xpt2046.h"
#endif
#endif

static const char *TAG = "BSP-Gen";

i2s_chan_handle_t i2s_tx_chan = NULL;
static i2s_chan_handle_t i2s_rx_chan = NULL;

#if defined(CONFIG_BSP_BUTTON_1_TYPE_ADC) || defined(CONFIG_BSP_BUTTON_2_TYPE_ADC) || defined(CONFIG_BSP_BUTTON_3_TYPE_ADC) || defined(CONFIG_BSP_BUTTON_4_TYPE_ADC) || defined(CONFIG_BSP_BUTTON_5_TYPE_ADC) || defined(CONFIG_BSP_BUTTON_6_TYPE_ADC) || defined(CONFIG_BSP_BUTTON_7_TYPE_ADC) || defined(CONFIG_BSP_BUTTON_8_TYPE_ADC) || defined(CONFIG_BSP_BUTTON_9_TYPE_ADC) || defined(CONFIG_BSP_BUTTON_10_TYPE_ADC)
static adc_oneshot_unit_handle_t bsp_adc_handle = NULL;
#endif

/* Can be used for i2s_std_gpio_config_t and/or i2s_std_config_t initialization */
#define BSP_I2S_GPIO_CFG       \
    {                          \
        .mclk = CONFIG_BSP_I2S_MCLK,  \
        .bclk = CONFIG_BSP_I2S_SCLK,  \
        .ws = CONFIG_BSP_I2S_LCLK,    \
        .dout = CONFIG_BSP_I2S_DOUT,  \
        .din = CONFIG_BSP_I2S_DSIN,   \
        .invert_flags = {      \
            .mclk_inv = false, \
            .bclk_inv = false, \
            .ws_inv = false,   \
        },                     \
    }

/* This configuration is used by default in bsp_audio_init() */
#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG,                                                                 \
    }

esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config)
{
    esp_err_t ret = ESP_FAIL;
    if (i2s_tx_chan && i2s_rx_chan) {
        /* Audio was initialized before */
        return ESP_OK;
    }

    /* Setup I2S peripheral */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(CONFIG_BSP_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    BSP_ERROR_CHECK_RETURN_ERR(i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan));

    /* Setup I2S channels */
    const i2s_std_config_t std_cfg_default = BSP_I2S_DUPLEX_MONO_CFG(44100);
    const i2s_std_config_t *p_i2s_cfg = &std_cfg_default;
    if (i2s_config != NULL) {
        p_i2s_cfg = i2s_config;
    }

    if (i2s_tx_chan != NULL) {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_chan, p_i2s_cfg), err, TAG, "I2S channel initialization failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_tx_chan), err, TAG, "I2S enabling failed");
    }

    if (i2s_rx_chan != NULL) {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_rx_chan, p_i2s_cfg), err, TAG, "I2S channel initialization failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_rx_chan), err, TAG, "I2S enabling failed");
    }

    return ESP_OK;

err:
    if (i2s_tx_chan) {
        i2s_del_channel(i2s_tx_chan);
    }

    if (i2s_rx_chan) {
        i2s_del_channel(i2s_rx_chan);
    }

    return ret;
}

#if CONFIG_BSP_DISPLAY_ENABLED
static lv_display_t *disp;
static lv_indev_t *disp_indev = NULL;
#endif

#if CONFIG_BSP_TOUCH_ENABLED
static esp_lcd_touch_handle_t tp;   // LCD touch handle
#endif

/**
 * @brief I2C handle for BSP usage
 *
 * In IDF v5.4 you can call i2c_master_get_bus_handle(BSP_I2C_NUM, i2c_master_bus_handle_t *ret_handle)
 * from #include "esp_private/i2c_platform.h" to get this handle
 *
 * For IDF 5.2 and 5.3 you must call bsp_i2c_get_handle()
 */
static i2c_master_bus_handle_t i2c_handle = NULL;
static bool i2c_initialized = false;
sdmmc_card_t *bsp_sdcard = NULL;    // Global uSD card handler
extern blink_step_t const *bsp_led_blink_defaults_lists[];

typedef enum {
    BSP_BUTTON_TYPE_GPIO,
    BSP_BUTTON_TYPE_ADC
} bsp_button_type_t;

typedef struct {
    bsp_button_type_t type;
    union {
        button_gpio_config_t gpio;
        button_adc_config_t  adc;
    } cfg;
} bsp_button_config_t;

static const bsp_button_config_t bsp_button_config[] = {
#if CONFIG_BSP_BUTTONS_NUM > 0
#if CONFIG_BSP_BUTTON_1_TYPE_GPIO
    {
        .type = BSP_BUTTON_TYPE_GPIO,
        .cfg.gpio = {
            .gpio_num = CONFIG_BSP_BUTTON_1_GPIO,
            .active_level = CONFIG_BSP_BUTTON_1_LEVEL,
        }
    },
#elif CONFIG_BSP_BUTTON_1_TYPE_ADC
    {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_channel = CONFIG_BSP_BUTTON_1_ADC_CHANNEL,
            .button_index = BSP_BUTTON_1,
#if CONFIG_HW_ODROID_GO
            .min = 1025,
            .max = 3073
#else
            .min = (CONFIG_BSP_BUTTON_1_ADC_VALUE - 100),
            .max = (CONFIG_BSP_BUTTON_1_ADC_VALUE + 100)
#endif
        }
    },
#endif // CONFIG_BSP_BUTTON_1_TYPE_x
#endif // CONFIG_BSP_BUTTONS_NUM >= 0
    
#if CONFIG_BSP_BUTTONS_NUM > 1
#if CONFIG_BSP_BUTTON_2_TYPE_GPIO
    {
        .type = BSP_BUTTON_TYPE_GPIO,
        .cfg.gpio = {
            .gpio_num = CONFIG_BSP_BUTTON_2_GPIO,
            .active_level = CONFIG_BSP_BUTTON_2_LEVEL,
        }
    },
#elif CONFIG_BSP_BUTTON_2_TYPE_ADC
    {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_channel = CONFIG_BSP_BUTTON_2_ADC_CHANNEL,
            .button_index = BSP_BUTTON_2,
#if CONFIG_HW_ODROID_GO
            .min = 1025,
            .max = 3073
#else
            .min = (CONFIG_BSP_BUTTON_2_ADC_VALUE - 100),
            .max = (CONFIG_BSP_BUTTON_2_ADC_VALUE + 100)
#endif
        }
    },
#endif // CONFIG_BSP_BUTTON_2_TYPE_x
#endif // CONFIG_BSP_BUTTONS_NUM >= 1
    
#if CONFIG_BSP_BUTTONS_NUM > 2
#if CONFIG_BSP_BUTTON_3_TYPE_GPIO
    {
        .type = BSP_BUTTON_TYPE_GPIO,
        .cfg.gpio = {
            .gpio_num = CONFIG_BSP_BUTTON_3_GPIO,
            .active_level = CONFIG_BSP_BUTTON_3_LEVEL,
        }
    },
#elif CONFIG_BSP_BUTTON_3_TYPE_ADC
    {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_channel = CONFIG_BSP_BUTTON_3_ADC_CHANNEL,
            .button_index = BSP_BUTTON_3,
#if CONFIG_HW_ODROID_GO
            .min = 1025,
            .max = 3073
#else
            .min = (CONFIG_BSP_BUTTON_3_ADC_VALUE - 100),
            .max = (CONFIG_BSP_BUTTON_3_ADC_VALUE + 100)
#endif
        }
    },
#endif // CONFIG_BSP_BUTTON_3_TYPE_x
#endif // CONFIG_BSP_BUTTONS_NUM >= 2
    
#if CONFIG_BSP_BUTTONS_NUM > 3
#if CONFIG_BSP_BUTTON_4_TYPE_GPIO
    {
        .type = BSP_BUTTON_TYPE_GPIO,
        .cfg.gpio = {
            .gpio_num = CONFIG_BSP_BUTTON_4_GPIO,
            .active_level = CONFIG_BSP_BUTTON_4_LEVEL,
        }
    },
#elif CONFIG_BSP_BUTTON_4_TYPE_ADC
    {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_channel = CONFIG_BSP_BUTTON_4_ADC_CHANNEL,
            .button_index = BSP_BUTTON_4,
#if CONFIG_HW_ODROID_GO
            .min = 1025,
            .max = 3073
#else
            .min = (CONFIG_BSP_BUTTON_4_ADC_VALUE - 100),
            .max = (CONFIG_BSP_BUTTON_4_ADC_VALUE + 100)
#endif
        }
    },
#endif // CONFIG_BSP_BUTTON_4_TYPE_x
#endif // CONFIG_BSP_BUTTONS_NUM >= 3
    
#if CONFIG_BSP_BUTTONS_NUM > 4
#if CONFIG_BSP_BUTTON_5_TYPE_GPIO
    {
        .type = BSP_BUTTON_TYPE_GPIO,
        .cfg.gpio = {
            .gpio_num = CONFIG_BSP_BUTTON_5_GPIO,
            .active_level = CONFIG_BSP_BUTTON_5_LEVEL,
        }
    },
#elif CONFIG_BSP_BUTTON_5_TYPE_ADC
    {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_channel = CONFIG_BSP_BUTTON_5_ADC_CHANNEL,
            .button_index = BSP_BUTTON_5,
#if CONFIG_HW_ODROID_GO
            .min = 1025,
            .max = 3073
#else
            .min = (CONFIG_BSP_BUTTON_5_ADC_VALUE - 100),
            .max = (CONFIG_BSP_BUTTON_5_ADC_VALUE + 100)
#endif
        }
    }
#endif // CONFIG_BSP_BUTTON_5_TYPE_x
#endif // CONFIG_BSP_BUTTONS_NUM >= 4
    
#if CONFIG_BSP_BUTTONS_NUM > 5
#if CONFIG_BSP_BUTTON_6_TYPE_GPIO
    {
        .type = BSP_BUTTON_TYPE_GPIO,
        .cfg.gpio = {
            .gpio_num = CONFIG_BSP_BUTTON_6_GPIO,
            .active_level = CONFIG_BSP_BUTTON_6_LEVEL,
        }
    },
#elif CONFIG_BSP_BUTTON_6_TYPE_ADC
    {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_channel = CONFIG_BSP_BUTTON_6_ADC_CHANNEL,
            .button_index = BSP_BUTTON_6,
#if CONFIG_HW_ODROID_GO
            .min = 1025,
            .max = 3073
#else
            .min = (CONFIG_BSP_BUTTON_6_ADC_VALUE - 100),
            .max = (CONFIG_BSP_BUTTON_6_ADC_VALUE + 100)
#endif
        }
    }
#endif // CONFIG_BSP_BUTTON_6_TYPE_x
#endif // CONFIG_BSP_BUTTONS_NUM >= 5
    
#if CONFIG_BSP_BUTTONS_NUM > 6
#if CONFIG_BSP_BUTTON_7_TYPE_GPIO
    {
        .type = BSP_BUTTON_TYPE_GPIO,
        .cfg.gpio = {
            .gpio_num = CONFIG_BSP_BUTTON_7_GPIO,
            .active_level = CONFIG_BSP_BUTTON_7_LEVEL,
        }
    },
#elif CONFIG_BSP_BUTTON_7_TYPE_ADC
    {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_channel = CONFIG_BSP_BUTTON_7_ADC_CHANNEL,
            .button_index = BSP_BUTTON_7,
#if CONFIG_HW_ODROID_GO
            .min = 1025,
            .max = 3073
#else
            .min = (CONFIG_BSP_BUTTON_7_ADC_VALUE - 100),
            .max = (CONFIG_BSP_BUTTON_7_ADC_VALUE + 100)
#endif
        }
    }
#endif // CONFIG_BSP_BUTTON_7_TYPE_x
#endif // CONFIG_BSP_BUTTONS_NUM >= 7
    
#if CONFIG_BSP_BUTTONS_NUM > 7
#if CONFIG_BSP_BUTTON_8_TYPE_GPIO
    {
        .type = BSP_BUTTON_TYPE_GPIO,
        .cfg.gpio = {
            .gpio_num = CONFIG_BSP_BUTTON_8_GPIO,
            .active_level = CONFIG_BSP_BUTTON_8_LEVEL,
        }
    },
#elif CONFIG_BSP_BUTTON_8_TYPE_ADC
    {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_channel = CONFIG_BSP_BUTTON_8_ADC_CHANNEL,
            .button_index = BSP_BUTTON_8,
#if CONFIG_HW_ODROID_GO
            .min = 1025,
            .max = 3073
#else
            .min = (CONFIG_BSP_BUTTON_8_ADC_VALUE - 100),
            .max = (CONFIG_BSP_BUTTON_8_ADC_VALUE + 100)
#endif
        }
    }
#endif // CONFIG_BSP_BUTTON_8_TYPE_x
#endif // CONFIG_BSP_BUTTONS_NUM >= 7

#if CONFIG_BSP_BUTTONS_NUM > 8
#if CONFIG_BSP_BUTTON_9_TYPE_GPIO
    {
        .type = BSP_BUTTON_TYPE_GPIO,
        .cfg.gpio = {
            .gpio_num = CONFIG_BSP_BUTTON_9_GPIO,
            .active_level = CONFIG_BSP_BUTTON_9_LEVEL,
        }
    },
#elif CONFIG_BSP_BUTTON_9_TYPE_ADC
    {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_channel = CONFIG_BSP_BUTTON_9_ADC_CHANNEL,
            .button_index = BSP_BUTTON_9,
#if CONFIG_HW_ODROID_GO
            .min = 1025,
            .max = 3073
#else
            .min = (CONFIG_BSP_BUTTON_9_ADC_VALUE - 100),
            .max = (CONFIG_BSP_BUTTON_9_ADC_VALUE + 100)
#endif
        }
    }
#endif // CONFIG_BSP_BUTTON_9_TYPE_x
#endif // CONFIG_BSP_BUTTONS_NUM >= 8

#if CONFIG_BSP_BUTTONS_NUM > 9
#if CONFIG_BSP_BUTTON_10_TYPE_GPIO
    {
        .type = BSP_BUTTON_TYPE_GPIO,
        .cfg.gpio = {
            .gpio_num = CONFIG_BSP_BUTTON_10_GPIO,
            .active_level = CONFIG_BSP_BUTTON_10_LEVEL,
        }
    },
#elif CONFIG_BSP_BUTTON_10_TYPE_ADC
    {
        .type = BSP_BUTTON_TYPE_ADC,
        .cfg.adc = {
            .adc_channel = CONFIG_BSP_BUTTON_10_ADC_CHANNEL,
            .button_index = BSP_BUTTON_10,
#if CONFIG_HW_ODROID_GO
            .min = 1025,
            .max = 3073
#else
            .min = (CONFIG_BSP_BUTTON_10_ADC_VALUE - 100),
            .max = (CONFIG_BSP_BUTTON_10_ADC_VALUE + 100)
#endif
        }
    }
#endif // CONFIG_BSP_BUTTON_10_TYPE_x
#endif // CONFIG_BSP_BUTTONS_NUM >= 9
};

#if CONFIG_BSP_LED_TYPE_GPIO
static led_indicator_gpio_config_t bsp_leds_gpio_config[] = {
    {
#if CONFIG_BSP_LED_1_LEVEL && CONFIG_BSP_LED_1_GPIO
        .is_active_level_high = CONFIG_BSP_LED_1_LEVEL,
        .gpio_num = CONFIG_BSP_LED_1_GPIO,
#endif
    },
    {
#if CONFIG_BSP_LED_2_LEVEL && CONFIG_BSP_LED_2_GPIO
        .is_active_level_high = CONFIG_BSP_LED_2_LEVEL,
        .gpio_num = CONFIG_BSP_LED_2_GPIO,
#endif
    },
    {
#if CONFIG_BSP_LED_3_LEVEL && CONFIG_BSP_LED_3_GPIO
        .is_active_level_high = CONFIG_BSP_LED_3_LEVEL,
        .gpio_num = CONFIG_BSP_LED_3_GPIO,
#endif
    },
    {
#if CONFIG_BSP_LED_4_LEVEL && CONFIG_BSP_LED_4_GPIO
        .is_active_level_high = CONFIG_BSP_LED_4_LEVEL,
        .gpio_num = CONFIG_BSP_LED_4_GPIO,
#endif
    },
    {
#if CONFIG_BSP_LED_5_LEVEL && CONFIG_BSP_LED_5_GPIO
        .is_active_level_high = CONFIG_BSP_LED_5_LEVEL,
        .gpio_num = CONFIG_BSP_LED_5_GPIO,
#endif
    }
};
#endif // CONFIG_BSP_LED_TYPE_GPIO

#if CONFIG_BSP_LED_TYPE_RGB && CONFIG_BSP_LEDS_NUM > 0
static const led_strip_config_t bsp_leds_rgb_strip_config = {
    .strip_gpio_num = CONFIG_BSP_LED_RGB_GPIO,   // The GPIO that connected to the LED strip's data line
    .max_leds = BSP_LED_NUM,                  // The number of LEDs in the strip,
    .led_model = LED_MODEL_WS2812,            // LED strip model
    .flags.invert_out = false,                // whether to invert the output signal
};

#if CONFIG_BSP_LED_RGB_BACKEND_RMT
static const led_strip_rmt_config_t bsp_leds_rgb_rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    .rmt_channel = 0,
#else
    .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000,     // RMT counter clock frequency = 10MHz
    .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
};
#elif CONFIG_BSP_LED_RGB_BACKEND_SPI
static led_strip_spi_config_t bsp_leds_rgb_spi_config = {
    .spi_bus = SPI2_HOST,
    .flags.with_dma = true,
};
#else
#error "unsupported LED strip backend"
#endif

static led_indicator_strips_config_t bsp_leds_rgb_config = {
    .led_strip_cfg = bsp_leds_rgb_strip_config,
#if CONFIG_BSP_LED_RGB_BACKEND_RMT
    .led_strip_driver = LED_STRIP_RMT,
    .led_strip_rmt_cfg = bsp_leds_rgb_rmt_config,
#elif CONFIG_BSP_LED_RGB_BACKEND_SPI
    .led_strip_driver = LED_STRIP_SPI,
    .led_strip_spi_cfg = bsp_leds_rgb_spi_config,
#endif
};

#elif CONFIG_BSP_LED_TYPE_RGB_CLASSIC && CONFIG_BSP_LEDS_NUM > 0 // CONFIG_BSP_LED_TYPE_RGB_CLASSIC

static led_indicator_rgb_config_t bsp_leds_rgb_config = {
    .is_active_level_high = CONFIG_BSP_LED_RGB_CLASSIC_LEVEL,
    .timer_num = LEDC_TIMER_0,
    .red_gpio_num = CONFIG_BSP_LED_RGB_RED_GPIO,
    .green_gpio_num = CONFIG_BSP_LED_RGB_GREEN_GPIO,
    .blue_gpio_num = CONFIG_BSP_LED_RGB_BLUE_GPIO,
    .red_channel = LEDC_CHANNEL_0,
    .green_channel = LEDC_CHANNEL_1,
    .blue_channel = LEDC_CHANNEL_2,
};

#endif // CONFIG_BSP_LED_TYPE_RGB

static const led_indicator_config_t bsp_leds_config[BSP_LED_NUM] = {
#if CONFIG_BSP_LED_TYPE_RGB
    {
        .mode = LED_STRIPS_MODE,
        .led_indicator_strips_config = &bsp_leds_rgb_config,
        .blink_lists = bsp_led_blink_defaults_lists,
        .blink_list_num = BSP_LED_MAX,
    },
#elif CONFIG_BSP_LED_TYPE_RGB_CLASSIC
    {
        .mode = LED_RGB_MODE,
        .led_indicator_rgb_config = &bsp_leds_rgb_config,
        .blink_lists = bsp_led_blink_defaults_lists,
        .blink_list_num = BSP_LED_MAX,
    },
#elif CONFIG_BSP_LED_TYPE_GPIO

#if CONFIG_BSP_LEDS_NUM > 0
    {
        .mode = LED_GPIO_MODE,
        .led_indicator_gpio_config = &bsp_leds_gpio_config[0],
        .blink_lists = bsp_led_blink_defaults_lists,
        .blink_list_num = BSP_LED_MAX,
    },
#endif  // CONFIG_BSP_LEDS_NUM > 0
#if CONFIG_BSP_LEDS_NUM > 1
    {
        .mode = LED_GPIO_MODE,
        .led_indicator_gpio_config = &bsp_leds_gpio_config[1],
        .blink_lists = bsp_led_blink_defaults_lists,
        .blink_list_num = BSP_LED_MAX,
    },
#endif  // CONFIG_BSP_LEDS_NUM > 1
#if CONFIG_BSP_LEDS_NUM > 2
    {
        .mode = LED_GPIO_MODE,
        .led_indicator_gpio_config = &bsp_leds_gpio_config[2],
        .blink_lists = bsp_led_blink_defaults_lists,
        .blink_list_num = BSP_LED_MAX,
    },
#endif  // CONFIG_BSP_LEDS_NUM > 2
#if CONFIG_BSP_LEDS_NUM > 3
    {
        .mode = LED_GPIO_MODE,
        .led_indicator_gpio_config = &bsp_leds_gpio_config[3],
        .blink_lists = bsp_led_blink_defaults_lists,
        .blink_list_num = BSP_LED_MAX,
    },
#endif  // CONFIG_BSP_LEDS_NUM > 3
#if CONFIG_BSP_LEDS_NUM > 4
    {
        .mode = LED_GPIO_MODE,
        .led_indicator_gpio_config = &bsp_leds_gpio_config[4],
        .blink_lists = bsp_led_blink_defaults_lists,
        .blink_list_num = BSP_LED_MAX,
    },
#endif  // CONFIG_BSP_LEDS_NUM > 4
#endif // CONFIG_BSP_LED_TYPE_RGB/CONFIG_BSP_LED_TYPE_GPIO
};

esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t i2c_config = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
#ifdef CONFIG_BSP_I2C_GPIO_PULLUP
        .flags.enable_internal_pullup = 1,
#endif
    };

    if ((BSP_I2C_SDA != -1) && (BSP_I2C_SCL != -1))
    {
        BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_config, &i2c_handle));
    }

    i2c_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    if ((BSP_I2C_SDA != -1) && (BSP_I2C_SCL != -1))
    {
        BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_handle));
    }

    i2c_initialized = false;
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    bsp_i2c_init();
    return i2c_handle;
}

esp_err_t bsp_littlefs_mount(void)
{
    int ret_val = ESP_OK;

    esp_vfs_littlefs_conf_t conf = {
        .base_path = CONFIG_BSP_LITTLEFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_LITTLEFS_PARTITION_LABEL,
#ifdef CONFIG_BSP_LITTLEFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .dont_mount = false,
    };

    // Use the API to mount and possibly format the file system
    ret_val = esp_vfs_littlefs_register(&conf);

    BSP_ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

esp_err_t bsp_littlefs_unmount(void)
{
    return esp_vfs_littlefs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
#ifdef CONFIG_BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

    BSP_ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

esp_err_t bsp_sdcard_mount(void)
{
#if SOC_SDMMC_HOST_SUPPORTED
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = CONFIG_BSP_SD_MAX_FILES,
        .allocation_unit_size = 16 * 1024
    };

#if CONFIG_SOC_SDSPI
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
#if CONFIG_IDF_TARGET_ESP32S3
    host.slot = SPI3_HOST;
#else
    host.slot = SPI2_HOST;
#endif
    host.max_freq_khz = 5000;
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_HW_SD_PIN_NUM_MOSI,
        .miso_io_num = CONFIG_HW_SD_PIN_NUM_MISO,
        .sclk_io_num = CONFIG_HW_SD_PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ret;
    }
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CONFIG_HW_SD_PIN_NUM_CS;
    slot_config.host_id = host.slot;
    return esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &bsp_sdcard);
#else
    const sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    const sdmmc_slot_config_t slot_config = {
#if SOC_SDMMC_USE_GPIO_MATRIX
        .clk = BSP_SD_CLK,
        .cmd = BSP_SD_CMD,
        .d0 = BSP_SD_D0,
        .d1 = BSP_SD_D1,
        .d2 = BSP_SD_D2,
        .d3 = BSP_SD_D3,
        .d4 = GPIO_NUM_NC,
        .d5 = GPIO_NUM_NC,
        .d6 = GPIO_NUM_NC,
        .d7 = GPIO_NUM_NC,
#endif
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 1,
        .flags = 0,
    };

    return esp_vfs_fat_sdmmc_mount(BSP_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &bsp_sdcard);
#endif
#else
    return ESP_OK;
#endif // SOC_SDMMC_HOST_SUPPORTED
}

esp_err_t bsp_sdcard_unmount(void)
{
#if SOC_SDMMC_HOST_SUPPORTED
    return esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, bsp_sdcard);
#else
    return ESP_OK;
#endif // SOC_SDMMC_HOST_SUPPORTED
}

#if CONFIG_BSP_DISPLAY_ENABLED
// Bit number used to represent command and parameter
#define LCD_CMD_BITS           CONFIG_BSP_DISPLAY_CMD_BITS
#define LCD_PARAM_BITS         CONFIG_BSP_DISPLAY_PARAM_BITS

esp_err_t bsp_display_brightness_init(void)
{
#if !CONFIG_BSP_DISPLAY_DRIVER_QEMU
#if CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&LCD_backlight_channel));
#endif
#endif
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
#if !CONFIG_BSP_DISPLAY_DRIVER_QEMU
#if CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

#if CONFIG_BSP_DISPLAY_BRIGHTNESS_INVERT
    brightness_percent = (100 - brightness_percent);
#endif

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH, duty_cycle));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH));
#endif
#endif

    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    assert(config != NULL && config->max_transfer_sz > 0);

#if CONFIG_BSP_DISPLAY_DRIVER_QEMU
    (void)ret_io;
    esp_lcd_rgb_qemu_config_t qemuconf = { CONFIG_BSP_DISPLAY_WIDTH, CONFIG_BSP_DISPLAY_HEIGHT, BSP_LCD_BITS_PER_PIXEL };
    ESP_GOTO_ON_ERROR(esp_lcd_new_rgb_qemu(&qemuconf, ret_panel), err, TAG, "New QEMU panel failed");
    ESP_LOGI(TAG, "Initialize LCD: QEMU");
#else
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");

    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_PCLK,
        .mosi_io_num = BSP_LCD_DATA0,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = config->max_transfer_sz,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    ESP_LOGD(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_CS,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_config, ret_io), err, TAG, "New panel IO failed");

    ESP_LOGD(TAG, "Install LCD driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .color_space = BSP_LCD_COLOR_SPACE,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
    };
#if CONFIG_BSP_DISPLAY_DRIVER_ST7789
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_st7789(*ret_io, &panel_config, ret_panel), err, TAG, "New panel failed");
    ESP_LOGI(TAG, "Initialize LCD: ST7789");
#elif CONFIG_BSP_DISPLAY_DRIVER_ILI9341
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_ili9341(*ret_io, &panel_config, ret_panel), err, TAG, "New panel failed");
    ESP_LOGI(TAG, "Initialize LCD: ILI9341");
#elif CONFIG_BSP_DISPLAY_DRIVER_GC9A01
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_gc9a01(*ret_io, &panel_config, ret_panel), err, TAG, "New panel failed");
    ESP_LOGI(TAG, "Initialize LCD: GC9A01");
#endif
#endif

    esp_lcd_panel_reset(*ret_panel);
    esp_lcd_panel_init(*ret_panel);

    bool disp_swap_xy = false;
    bool disp_mirror_x = false;
    bool disp_mirror_y = false;
    bool disp_invert_color = false;
#if CONFIG_BSP_DISPLAY_ROTATION_SWAP_XY
    disp_swap_xy = true;
#endif
#if CONFIG_BSP_DISPLAY_ROTATION_MIRROR_X
    disp_mirror_x = true;
#endif
#if CONFIG_BSP_DISPLAY_ROTATION_MIRROR_Y
    disp_mirror_y = true;
#endif
#if CONFIG_BSP_DISPLAY_INVERT_COLOR
    disp_invert_color = true;
#endif

    esp_lcd_panel_mirror(*ret_panel, disp_mirror_x, disp_mirror_y);
    esp_lcd_panel_swap_xy(*ret_panel, disp_swap_xy);
    esp_lcd_panel_invert_color(*ret_panel, disp_invert_color);
    return ret;

err:
    if (*ret_panel) {
        esp_lcd_panel_del(*ret_panel);
    }
                         
#if !CONFIG_BSP_DISPLAY_DRIVER_QEMU
    if (*ret_io) {
        esp_lcd_panel_io_del(*ret_io);
    }
    spi_bus_free(BSP_LCD_SPI_NUM);
#endif

    return ret;
}

static lv_display_t *bsp_display_lcd_init(void)
{
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = NULL;
    const bsp_display_config_t bsp_disp_cfg = {
        .max_transfer_sz = (BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT) * sizeof(uint16_t),
    };
    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new(&bsp_disp_cfg, &panel_handle, &io_handle));

    esp_lcd_panel_disp_on_off(panel_handle, true);

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
#if CONFIG_BSP_LCD_DRAW_BUF_DOUBLE
        .double_buffer = 1,
#else
        .double_buffer = 0,
#endif
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .monochrome = false,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
#if CONFIG_BSP_DISPLAY_ROTATION_SWAP_XY
            .swap_xy = true,
#endif
#if CONFIG_BSP_DISPLAY_ROTATION_MIRROR_X
            .mirror_x = true,
#endif
#if CONFIG_BSP_DISPLAY_ROTATION_MIRROR_Y
            .mirror_y = true,
#endif
        },
        .flags = {
            .buff_dma = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = (BSP_LCD_BIGENDIAN ? true : false),
#endif
        }
    };
#if BSP_LCD_H_OFFSET || BSP_LCD_V_OFFSET
    esp_lcd_panel_set_gap(panel_handle, (BSP_LCD_H_OFFSET), (BSP_LCD_V_OFFSET));
#endif
    return lvgl_port_add_disp(&disp_cfg);
}

#if CONFIG_BSP_TOUCH_ENABLED
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    /* Initilize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    /* Initialize touch */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = BSP_LCD_TOUCH_RST,
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
#if CONFIG_BSP_TOUCH_ROTATION_SWAP_XY
            .swap_xy = true,
#endif
#if CONFIG_BSP_TOUCH_ROTATION_MIRROR_X
            .mirror_x = true,
#endif
#if CONFIG_BSP_TOUCH_ROTATION_MIRROR_Y
            .mirror_y = true,
#endif
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
#if CONFIG_BSP_TOUCH_DRIVER_TT21100
    ESP_LOGI(TAG, "Initialize LCD Touch: TT21100");
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_TT21100_CONFIG();
    tp_io_config.scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ; // This parameter was introduced together with I2C Driver-NG in IDF v5.2
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_tt21100(tp_io_handle, &tp_cfg, ret_touch);
#elif CONFIG_BSP_TOUCH_DRIVER_GT1151
    ESP_LOGI(TAG, "Initialize LCD Touch: GT1151");
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT1151_CONFIG();
    tp_io_config.scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ; // This parameter was introduced together with I2C Driver-NG in IDF v5.2
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_gt1151(tp_io_handle, &tp_cfg, ret_touch);
#elif CONFIG_BSP_TOUCH_DRIVER_GT911
    ESP_LOGI(TAG, "Initialize LCD Touch: GT911");
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ; // This parameter was introduced together with I2C Driver-NG in IDF v5.2
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);
#elif CONFIG_BSP_TOUCH_DRIVER_CST816S
    ESP_LOGI(TAG, "Initialize LCD Touch: CST816S");
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_config.scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ; // This parameter was introduced together with I2C Driver-NG in IDF v5.2
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, ret_touch);
#elif CONFIG_BSP_TOUCH_DRIVER_FT5X06
    ESP_LOGI(TAG, "Initialize LCD Touch: FT5X06");
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5X06_CONFIG();
    tp_io_config.scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ; // This parameter was introduced together with I2C Driver-NG in IDF v5.2
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, ret_touch);
#elif CONFIG_BSP_TOUCH_DRIVER_XPT2046
    ESP_LOGI(TAG, "Initialize LCD Touch: XPT2046");
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_BSP_TOUCH_MISO_GPIO,
        .mosi_io_num = CONFIG_BSP_TOUCH_MOSI_GPIO,
        .sclk_io_num = CONFIG_BSP_TOUCH_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32
    };
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    esp_lcd_panel_io_spi_config_t tp_io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(CONFIG_BSP_TOUCH_CS_GPIO);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI3_HOST, &tp_io_config, &tp_io_handle));
    return esp_lcd_touch_new_spi_xpt2046(tp_io_handle, &tp_cfg, ret_touch);
#endif
}

static lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(NULL, &tp));
    assert(tp);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = tp,
    };

    return lvgl_port_add_touch(&touch_cfg);
}
#endif //CONFIG_BSP_TOUCH_ENABLED

lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG()
    };
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_init(&cfg->lvgl_port_cfg));

    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_brightness_init());

    BSP_NULL_CHECK(disp = bsp_display_lcd_init(), NULL);
#if CONFIG_BSP_TOUCH_ENABLED
    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(disp), NULL);
#endif //CONFIG_BSP_TOUCH_ENABLED

    return disp;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation)
{
    lv_disp_set_rotation(disp, rotation);
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}
#endif //CONFIG_BSP_DISPLAY_ENABLED

esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size)
{
    esp_err_t ret = ESP_OK;
    const button_config_t btn_config = {0};
    if ((btn_array_size < BSP_BUTTON_NUM) ||
            (btn_array == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (btn_cnt) {
        *btn_cnt = 0;
    }
    for (int i = 0; i < BSP_BUTTON_NUM; i++) {
        if (bsp_button_config[i].type == BSP_BUTTON_TYPE_GPIO) {
            ret |= iot_button_new_gpio_device(&btn_config, &bsp_button_config[i].cfg.gpio, &btn_array[i]);
        } else if (bsp_button_config[i].type == BSP_BUTTON_TYPE_ADC) {
            ret |= iot_button_new_adc_device(&btn_config, &bsp_button_config[i].cfg.adc, &btn_array[i]);
        } else {
            ESP_LOGW(TAG, "Unsupported button type!");
        }

        if (btn_cnt) {
            (*btn_cnt)++;
        }
    }
    return ret;
}

esp_err_t bsp_led_indicator_create(led_indicator_handle_t led_array[], int *led_cnt, int led_array_size)
{
    esp_err_t ret = ESP_OK;
    if ((led_array_size < BSP_LED_NUM) ||
            (led_array == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (led_cnt) {
        *led_cnt = 0;
    }
    for (int i = 0; i < BSP_LED_NUM; i++) {
        led_array[i] = led_indicator_create(&bsp_leds_config[i]);
        if (led_array[i] == NULL) {
            ret = ESP_FAIL;
            break;
        }
        if (led_cnt) {
            (*led_cnt)++;
        }
    }
    return ret;
}

esp_err_t bsp_led_set(led_indicator_handle_t handle, const bool on)
{
    if (on) {
        led_indicator_start(handle, BSP_LED_ON);
    } else {
        led_indicator_start(handle, BSP_LED_OFF);
    }

    return ESP_OK;
}

esp_err_t bsp_led_set_temperature(led_indicator_handle_t handle, const uint16_t temperature)
{
    return led_indicator_set_color_temperature(handle, temperature);
}
