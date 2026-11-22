#include <inttypes.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "remote-control";

void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "chip: %s rev %d.%d, %d cores",
             CONFIG_IDF_TARGET, chip.revision / 100, chip.revision % 100, chip.cores);
    ESP_LOGI(TAG, "flash: %" PRIu32 " MB", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "PSRAM: %u KB", (unsigned)(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024));
}
