
#include "opentyr.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern int main(int argc, const char *argv[]);

void tyrianTask(void *pvParameters)
{
//    heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);
//    spi_lcd_init();

    const char *argv[]={"opentyrian2000", NULL};
    main(1, argv);
}


//extern "C"
void app_main(void)
{
	xTaskCreatePinnedToCore(&tyrianTask, "tyrianTask", 34000, NULL, 5, NULL, 0);
}
