#include "SDL_system.h"

#if CONFIG_LITTLEFS
#include "esp_vfs.h"
#include "esp_littlefs.h"
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define VSPI_HOST SPI3_HOST
#define HSPI_HOST SPI2_HOST
#endif

const char *SDL_TAG = "SDL";

struct SDL_mutex
{
    pthread_mutex_t id;
#if FAKE_RECURSIVE_MUTEX
    int recursive;
    pthread_t owner;
#endif
};

#if 0
void SDL_ClearError(void)
{

}
#endif

void SDL_Delay(Uint32 ms)
{
    const TickType_t xDelay = ms / portTICK_PERIOD_MS;
    vTaskDelay( xDelay );
}

#if 0
char *SDL_GetError(void)
{
    return (char *)"";
}
#endif

int SDL_Init(Uint32 flags)
{
    if(flags == SDL_INIT_VIDEO)
        SDL_InitSubSystem(flags);
    return 0;
}

void SDL_Quit(void)
{

}

#if CONFIG_LITTLEFS
void listFiles(const char *dirname) {
    DIR *dir;
    struct dirent *entry;

    // Open the directory
    dir = opendir(dirname);
    if (!dir) {
        printf("Failed to open directory: %s\n", dirname);
        return;
    }

    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        struct stat entry_stat;
        char path[1024];

        // Build full path for stat
        snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);

        // Get entry status
        if (stat(path, &entry_stat) == -1) {
            printf("Failed to stat %s\n", path);
            continue;
        }

        // Check if it's a directory or a file
        if (S_ISDIR(entry_stat.st_mode)) {
            printf("[DIR]  %s\n", entry->d_name);
        } else if (S_ISREG(entry_stat.st_mode)) {
            printf("[FILE] %s (Size: %ld bytes)\n", entry->d_name, entry_stat.st_size);
        }
    }

    // Close the directory
    closedir(dir);
}

void SDL_InitSD(void) {
    printf("Initialising File System\n");

    // Define the LittleFS configuration
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/sd",
        .partition_label = "storage",
        .format_if_mount_failed = false,
        .dont_mount = false,
    };

    // Use the API to mount and possibly format the file system
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        printf("Failed to mount or format filesystem\n");
    } else {
        printf("Filesystem mounted\n");
        printf("Listing files in /:\n");
        listFiles("/sd");
    }
}
#else
void SDL_InitSD(void)
{
    esp_err_t ret;

	/*sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.command_timeout_ms = 3000;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    // https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/spi_master.html
    host.slot = CONFIG_HW_SD_PIN_NUM_MISO == 0 ? VSPI_HOST : HSPI_HOST;
    sdspi_device_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = CONFIG_HW_SD_PIN_NUM_MISO;
    slot_config.gpio_mosi = CONFIG_HW_SD_PIN_NUM_MOSI;
    slot_config.gpio_sck  = CONFIG_HW_SD_PIN_NUM_CLK;
    slot_config.gpio_cs   = CONFIG_HW_SD_PIN_NUM_CS;
	//slot_config.dma_channel = 1; //2*/
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    ESP_LOGI(SDL_TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(SDL_TAG, "Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = HSPI_HOST;
    host.max_freq_khz = 5000;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_HW_SD_PIN_NUM_MOSI,
        .miso_io_num = CONFIG_HW_SD_PIN_NUM_MISO,
        .sclk_io_num = CONFIG_HW_SD_PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    SDL_LockDisplay();
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(SDL_TAG, "Failed to initialize bus.");
        SDL_UnlockDisplay();
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CONFIG_HW_SD_PIN_NUM_CS;
    slot_config.host_id = host.slot;

    SDL_Delay(200);
    ESP_LOGI(SDL_TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount("/sd", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(SDL_TAG, "Failed to mount filesystem.");
        } else {
            ESP_LOGE(SDL_TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        SDL_UnlockDisplay();
        return;
    }
    SDL_UnlockDisplay();

    ESP_LOGI(SDL_TAG, "Init_SD: SD card opened.\n");
}
#endif

const SDL_version* SDL_Linked_Version()
{
    SDL_version *vers = malloc(sizeof(SDL_version));
    vers->major = SDL_MAJOR_VERSION;                 
    vers->minor = SDL_MINOR_VERSION;                 
    vers->patch = SDL_PATCHLEVEL;      
    return vers;
}

char *** allocateTwoDimenArrayOnHeapUsingMalloc(int row, int col)
{
	char ***ptr = malloc(row * sizeof(*ptr) + row * (col * sizeof **ptr) );

	int * const data = (int *)(ptr + row);
	for(int i = 0; i < row; i++)
		ptr[i] = (char **)(data + i * col);

	return ptr;
}

void SDL_DestroyMutex(SDL_mutex* mutex)
{

}

SDL_mutex* SDL_CreateMutex(void)
{
    SDL_mutex* mut = NULL;
    return mut;
}

int SDL_LockMutex(SDL_mutex* mutex)
{
    return 0;
}

int SDL_UnlockMutex(SDL_mutex* mutex)
{
    return 0;
}
