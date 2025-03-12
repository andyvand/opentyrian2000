#include "SDL_audio.h"

SDL_AudioSpec as;
//unsigned char *sdl_buffer;
unsigned char *sdl_buffer; //[SAMPLECOUNT * SAMPLESIZE * 2];
void *user_data;
bool paused = true;
bool locked = false;
SemaphoreHandle_t xSemaphoreAudio = NULL;

#ifdef CONFIG_HW_ODROID_GO
#define I2S_BCK_IO      (27)
#define I2S_WS_IO       (26)
#define I2S_DO_IO       (25)
#else
#define I2S_BCK_IO      (CONFIG_I2S_BCK_IO)
#define I2S_WS_IO       (CONFIG_I2S_WS_IO)
#define I2S_DO_IO       (CONFIG_I2S_DO_IO)
#endif
#define I2S_DI_IO       (-1)

static i2s_chan_handle_t tx_chan;        // I2S tx channel handler

/*IRAM_ATTR*/ void updateTask(void *arg)
{
  size_t w_bytes = SAMPLECOUNT*SAMPLESIZE*2;

  while (w_bytes == SAMPLECOUNT*SAMPLESIZE*2) {
    /* Here we load the target buffer repeatedly, until all the DMA buffers are preloaded */
    ESP_ERROR_CHECK(i2s_channel_preload_data(tx_chan, sdl_buffer, SAMPLECOUNT*SAMPLESIZE*2, &w_bytes));
  }
    
  ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

  while(1)
  {
	  if(!paused && /*xSemaphoreAudio != NULL*/ !locked ){
          (*as.callback)(NULL, sdl_buffer, SAMPLECOUNT*SAMPLESIZE );
          if (i2s_channel_write(tx_chan, sdl_buffer, SAMPLECOUNT*SAMPLESIZE*2, &w_bytes, 1000) == ESP_OK) {
              ESP_LOGI(SDL_TAG, "Write Task: i2s write %d bytes\n", w_bytes);
          } else {
              ESP_LOGI(SDL_TAG, "Write Task: i2s write failed\n");
          }
          vTaskDelay(pdMS_TO_TICKS(200));
      } else
		  vTaskDelay( 5 );
  }
}

void SDL_AudioInit()
{
	sdl_buffer = heap_caps_malloc(SAMPLECOUNT * SAMPLESIZE * 2, MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLERATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,    // some codecs may require mclk signal, this example doesn't need it
            .bclk = I2S_BCK_IO,
            .ws   = I2S_WS_IO,
            .dout = I2S_DO_IO,
            .din  = I2S_DI_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
}

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
	SDL_AudioInit();
	memset(obtained, 0, sizeof(SDL_AudioSpec)); 
	obtained->freq = SAMPLERATE;
	obtained->format = 16;
	obtained->channels = 1;
	obtained->samples = SAMPLECOUNT;
	obtained->callback = desired->callback;
	memcpy(&as,obtained,sizeof(SDL_AudioSpec));  

	//xSemaphoreAudio = xSemaphoreCreateBinary();
	xTaskCreatePinnedToCore(&updateTask, "updateTask", 10000, NULL, 3, NULL, 1);
    ESP_LOGI(SDL_TAG, "audio task started\n");
	return 0;
}

void SDL_PauseAudio(int pause_on)
{
	paused = pause_on;
}

void SDL_CloseAudio(void)
{
	  free(sdl_buffer);
}

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 src_format, Uint8 src_channels, int src_rate, Uint16 dst_format, Uint8 dst_channels, int dst_rate)
{
	cvt->len_mult = 1;
	return 0;
}

/*IRAM_ATTR*/ int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{

	Sint16 *sbuf = (Sint16 *)cvt->buf;
	Uint16 *ubuf = (Uint16 *)cvt->buf;

	int32_t dac0;
	int32_t dac1;

	for(int i = cvt->len-2; i >= 0; i-=2)
	{
		Sint16 range = sbuf[i/2] >> 8; 

		// Convert to differential output
		if (range > 127)
		{
			dac1 = (range - 127);
			dac0 = 127;
		}
		else if (range < -127)
		{
			dac1  = (range + 127);
			dac0 = -127;
		}
		else
		{
			dac1 = 0;
			dac0 = range;
		}

		dac0 += 0x80;
		dac1 = 0x80 - dac1;

		dac0 <<= 8;
		dac1 <<= 8;

		ubuf[i] = (int16_t)dac1;
        ubuf[i + 1] = (int16_t)dac0;
	}

	return 0;
}

void SDL_LockAudio(void)
{
	locked = true;
	//if( xSemaphoreAudio != NULL )
	//	xSemaphoreTake( xSemaphoreAudio, 100 );
}

void SDL_UnlockAudio(void)
{
    locked = false;
	//if( xSemaphoreAudio != NULL )
	//	 xSemaphoreGive( xSemaphoreAudio );
}

