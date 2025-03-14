#include "SDL_event.h"
#include "SDL_video.h"

#ifdef CONFIG_HW_ODROID_GO
//#include "driver/adc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#endif

#ifndef CONFIG_HW_ODROID_GO
#if CONFIG_TOUCH_ENABLED
#include "hal/spi_types.h"
#include "esp_lcd_touch_xpt2046.h"

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define VSPI_HOST SPI2_HOST
#define HSPI_HOST SPI3_HOST
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define TFT_VSPI_HOST VSPI_HOST
#elif CONFIG_HW_LCD_MISO_GPIO == 19
#define TFT_VSPI_HOST VSPI_HOST
#elif CONFIG_HW_LCD_MISO_GPIO == 2
#define TFT_VSPI_HOST HSPI_HOST
#else
#define TFT_VSPI_HOST SPI_HOST
#endif
#endif
#endif

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

typedef struct {
	int gpio;
	SDL_Scancode scancode;
    SDL_Keycode keycode;
} GPIOKeyMap;

#ifndef CONFIG_HW_ODROID_GO
#if CONFIG_TOUCH_ENABLED
esp_lcd_touch_handle_t tp = NULL;
esp_lcd_panel_io_handle_t tp_io_handle = NULL;
esp_lcd_panel_io_spi_config_t tp_io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(CONFIG_HW_TOUCH_PIN_NUM_CS_CUST);
#endif
#endif

int keyMode = 1;
//Mappings from buttons to keys
#ifdef CONFIG_HW_ODROID_GO
static const GPIOKeyMap keymap[2][6]={{
// Game    
	{CONFIG_HW_BUTTON_PIN_NUM_BUTTON1, SDL_SCANCODE_SPACE, SDLK_SPACE}, 	
	{CONFIG_HW_BUTTON_PIN_NUM_BUTTON2, SDL_SCANCODE_RETURN, SDLK_RETURN},	
	{CONFIG_HW_BUTTON_PIN_NUM_VOL, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE},		
	{CONFIG_HW_BUTTON_PIN_NUM_MENU, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE},	
	{CONFIG_HW_BUTTON_PIN_NUM_START, SDL_SCANCODE_LALT, SDLK_LALT},	
	{CONFIG_HW_BUTTON_PIN_NUM_SELECT, SDL_SCANCODE_LCTRL, SDLK_LCTRL},	
},
// Menu
{
	{CONFIG_HW_BUTTON_PIN_NUM_BUTTON1, SDL_SCANCODE_SPACE, SDLK_SPACE}, 	
	{CONFIG_HW_BUTTON_PIN_NUM_BUTTON2, SDL_SCANCODE_RETURN, SDLK_RETURN},			
	{CONFIG_HW_BUTTON_PIN_NUM_VOL, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE},	    
	{CONFIG_HW_BUTTON_PIN_NUM_MENU, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE},	
	{CONFIG_HW_BUTTON_PIN_NUM_START, SDL_SCANCODE_LALT, SDLK_LALT},	
	{CONFIG_HW_BUTTON_PIN_NUM_SELECT, SDL_SCANCODE_LCTRL, SDLK_LCTRL},		
}};
#else
static const GPIOKeyMap keymap[2][6]={{
// Game    
	{CONFIG_HW_BUTTON_PIN_NUM_UP, SDL_SCANCODE_UP, SDLK_UP},
	{CONFIG_HW_BUTTON_PIN_NUM_RIGHT, SDL_SCANCODE_RIGHT, SDLK_RIGHT},
 	{CONFIG_HW_BUTTON_PIN_NUM_DOWN, SDL_SCANCODE_DOWN, SDLK_DOWN},
	{CONFIG_HW_BUTTON_PIN_NUM_LEFT, SDL_SCANCODE_LEFT, SDLK_LEFT}, 

	{CONFIG_HW_BUTTON_PIN_NUM_BUTTON2, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE},			
	{CONFIG_HW_BUTTON_PIN_NUM_BUTTON1, SDL_SCANCODE_SPACE, SDLK_SPACE},   	
},
// Menu
{
	{CONFIG_HW_BUTTON_PIN_NUM_UP, SDL_SCANCODE_UP, SDLK_UP},
	{CONFIG_HW_BUTTON_PIN_NUM_RIGHT, SDL_SCANCODE_RIGHT, SDLK_RIGHT},
    {CONFIG_HW_BUTTON_PIN_NUM_DOWN, SDL_SCANCODE_DOWN, SDLK_DOWN},
	{CONFIG_HW_BUTTON_PIN_NUM_LEFT, SDL_SCANCODE_LEFT, SDLK_LEFT}, 

	{CONFIG_HW_BUTTON_PIN_NUM_BUTTON2, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE},			
	{CONFIG_HW_BUTTON_PIN_NUM_BUTTON1, SDL_SCANCODE_SPACE, SDLK_SPACE},   	
}};

static QueueHandle_t gpio_evt_queue = NULL;

typedef struct {
    Uint32 type;        /**< ::SDL_KEYDOWN or ::SDL_KEYUP */
    SDL_Scancode scancode;
    SDL_Scancode keycode;
} GPIOEvent;
#endif

#define ODROID_GAMEPAD_IO_X ADC_CHANNEL_6
#define ODROID_GAMEPAD_IO_Y ADC_CHANNEL_7

typedef struct
{
    uint8_t up;
    uint8_t right;
    uint8_t down;
    uint8_t left;
    uint8_t buttons[6];
} JoystickState;

JoystickState lastState = {0,0,0,0,{0,0,0,0,0,0}};

bool initInput = false;

int checkPin(int state, uint8_t *lastState, SDL_Scancode sc, SDL_Keycode kc, SDL_Event *event)
{
    if(state != *lastState)
    {
        *lastState = state;
        event->key.keysym.scancode = sc;
        event->key.keysym.sym = kc;
        event->key.type = state ? SDL_KEYDOWN : SDL_KEYUP;
        event->key.state = state ? SDL_PRESSED : SDL_RELEASED;
        return 1;
    }
    return 0;
}

int checkPinStruct(int i, uint8_t *lastState, SDL_Event *event)
{
    int state = 1-gpio_get_level(keymap[keyMode][i].gpio);
    if(state != *lastState)
    {
        *lastState = state;
        event->key.keysym.scancode = keymap[keyMode][i].scancode;
        event->key.keysym.sym = keymap[keyMode][i].keycode;
        event->key.type = state ? SDL_KEYDOWN : SDL_KEYUP;
        event->key.state = state ? SDL_PRESSED : SDL_RELEASED;
        return 1;
    }
    return 0;
}

#ifdef CONFIG_HW_ODROID_GO
adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
};
adc_oneshot_chan_cfg_t config = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
};

int readOdroidXY(SDL_Event * event)
{
    int joyX = 0;
    int joyY = 0;

    adc_oneshot_read(adc1_handle, ODROID_GAMEPAD_IO_X, &joyX);
    adc_oneshot_read(adc1_handle, ODROID_GAMEPAD_IO_Y, &joyY);

    JoystickState state;
    if (joyX > 2048 + 1024)
    {
        state.left = 1;
        state.right = 0;
    }
    else if (joyX > 1024)
    {
        state.left = 0;
        state.right = 1;
    }
    else
    {
        state.left = 0;
        state.right = 0;
    }

    if (joyY > 2048 + 1024)
    {
        state.up = 1;
        state.down = 0;
    }
    else if (joyY > 1024)
    {
        state.up = 0;
        state.down = 1;
    }
    else
    {
        state.up = 0;
        state.down = 0;
    }

    event->key.keysym.mod = 0;
    if(checkPin(state.up, &lastState.up, SDL_SCANCODE_UP, SDLK_UP, event))
        return 1;
    if(checkPin(state.down, &lastState.down, SDL_SCANCODE_DOWN, SDLK_DOWN, event))
        return 1;
    if(checkPin(state.left, &lastState.left, SDL_SCANCODE_LEFT, SDLK_LEFT, event))
        return 1;
    if(checkPin(state.right, &lastState.right, SDL_SCANCODE_RIGHT, SDLK_RIGHT, event))
        return 1;

    for(int i = 0; i < 6; i++)
        if(checkPinStruct(i, &lastState.buttons[i], event))
            return 1;

    return 0;
}
#endif

#ifndef CONFIG_HW_ODROID_GO
#if CONFIG_TOUCH_ENABLED
bool was_pressed = false;
#endif
#endif

int SDL_PollEvent(SDL_Event * event)
{
#ifndef CONFIG_HW_ODROID_GO
    GPIOEvent ev;
#endif

    if(!initInput)
        inputInit();

#ifndef CONFIG_HW_ODROID_GO
#if CONFIG_TOUCH_ENABLED
    uint16_t x[1];
    uint16_t y[1];
    uint16_t  strength[1];
    uint8_t count = 0;

    SDL_LockDisplay();
    event->motion.state = esp_lcd_touch_get_coordinates(tp, x, y, strength, &count, 1) ? SDL_PRESSED : SDL_RELEASED;
    SDL_UnlockDisplay();
    event->motion.type = event->motion.state == SDL_PRESSED ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;

    if (event->motion.state == SDL_PRESSED)
    {
        was_pressed = true;
        event->motion.x = x[0];
        event->motion.y = y[0];
    }
#endif
#endif

#ifndef CONFIG_HW_ODROID_GO
    if(xQueueReceive(gpio_evt_queue, &ev, 0)) {
        if ((ev.type == SDL_KEYDOWN) || ev.type == SDL_KEYUP)
        {
            event->type = ev.type == SDL_KEYDOWN ? SDL_KEYDOWN : SDL_KEYUP;
            event->key.keysym.sym = ev.keycode;
            event->key.keysym.scancode = ev.scancode;
            event->key.type = ev.type;
            event->key.keysym.mod = 0;
            event->key.state = ev.type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;     //< ::SDL_PRESSED or ::SDL_RELEASED
        }
        return 1;
    }
#else
    return readOdroidXY(event);
#endif

#ifndef CONFIG_HW_ODROID_GO
#if CONFIG_TOUCH_ENABLED
    if ((was_pressed == true) && (event->motion.state == SDL_RELEASED)
    {
        was_pressed = false;
        return 1;
    } else if (event->motion.state == SDL_PRESSED) {
        return 1;
    }
#endif
#endif

    return 0;
}

#ifndef CONFIG_HW_ODROID_GO
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    GPIOEvent event;
    event.type = gpio_get_level(gpio_num) == 0 ? SDL_KEYDOWN : SDL_KEYUP;
    for (int i=0; i < NELEMS(keymap[keyMode]); i++)
        if(keymap[keyMode][i].gpio == gpio_num)
        {
            event.scancode = keymap[keyMode][i].scancode;
            event.keycode = keymap[keyMode][i].keycode;
            xQueueSendFromISR(gpio_evt_queue, &event, NULL);
        }
}
#endif
/*
void gpioTask(void *arg) {
    uint32_t io_num;
	int level;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
        }
    }
}
*/
void inputInit()
{
	gpio_config_t io_conf;
    io_conf.pull_down_en = 0;

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;

    //bit mask of the pins, use GPIO... here
	for (int i=0; i < NELEMS(keymap[0]); i++)
    	if(i==0)
			io_conf.pin_bit_mask = (1ULL<<keymap[0][i].gpio);
		else
			io_conf.pin_bit_mask |= (1ULL<<keymap[0][i].gpio);

	io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
#ifndef CONFIG_HW_ODROID_GO
    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(GPIOEvent));
    //start gpio task
	//xTaskCreatePinnedToCore(&gpioTask, "GPIO", 1500, NULL, 7, NULL, 0);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_SHARED);

    //hook isr handler
	for (int i=0; i < NELEMS(keymap[0]); i++)
    	gpio_isr_handler_add(keymap[0][i].gpio, gpio_isr_handler, (void*) keymap[0][i].gpio);
#else
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ODROID_GAMEPAD_IO_X, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ODROID_GAMEPAD_IO_Y, &config));
#endif

	ESP_LOGI(SDL_TAG, "keyboard: GPIO task created.\n");

#ifndef CONFIG_HW_ODROID_GO
#if CONFIG_TOUCH_ENABLED
    SDL_LockDisplay();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TFT_VSPI_HOST, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
         .x_max = 240,
         .y_max = 320,
         .rst_gpio_num = -1,
         .int_gpio_num = -1,
         .flags = {
             .swap_xy = 1,
             .mirror_x = 0,
             .mirror_y = 0,
         },
    };

    ESP_LOGI(SDL_TAG, "Initialize touch controller XPT2046");
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tp_io_handle, &tp_cfg, &tp));
    SDL_UnlockDisplay();
#endif
#endif

    initInput = true;
}
