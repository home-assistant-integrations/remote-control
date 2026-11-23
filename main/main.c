#include <inttypes.h>
#include <stdbool.h>

#include "device_config.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "screen.h"
#include "setup_mode.h"
#include "ui.h"

static const char *TAG = "remote-control";

/* Push button of the rocker switch. Input-only pin, pulled up on the board;
 * reads low while pressed. */
#define SETUP_BUTTON_GPIO GPIO_NUM_38
#define SETUP_HOLD_MS     3000

static void log_chip(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "chip: %s rev %d.%d, %d cores", CONFIG_IDF_TARGET, chip.revision / 100,
             chip.revision % 100, chip.cores);
    ESP_LOGI(TAG, "flash: %" PRIu32 " MB", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "PSRAM: %u KB", (unsigned)(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024));
}

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

/* True if the push button is held for SETUP_HOLD_MS from the start. */
static bool setup_button_held(void)
{
    gpio_set_direction(SETUP_BUTTON_GPIO, GPIO_MODE_INPUT);
    if (gpio_get_level(SETUP_BUTTON_GPIO) != 0) {
        return false;
    }
    ESP_LOGI(TAG, "push button held: keep holding for setup mode");
    for (int t = 0; t < SETUP_HOLD_MS; t += 50) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (gpio_get_level(SETUP_BUTTON_GPIO) != 0) {
            ESP_LOGI(TAG, "push button released early: normal start");
            return false;
        }
    }
    return true;
}

static void on_setup_ready(const char *ssid, const char *password, const char *url)
{
    screen_show_setup(ssid, password, url);
}

static void on_setup_saved(void)
{
    screen_show_message("Configuration saved, restarting");
}

static const setup_callbacks_t setup_callbacks = {
    .on_ready = on_setup_ready,
    .on_saved = on_setup_saved,
};

void app_main(void)
{
    screen_init(); /* first: holds the power on when running on battery */
    log_chip();

    init_nvs();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(config_fs_init());

    /* The setup dialog in the status bar leaves a one-time request in NVS. */
    bool requested = ui_take_setup_request();
    bool button = !requested && setup_button_held();
    static config_t cfg;
    esp_err_t err = config_load(&cfg);

    if (requested || button || err != ESP_OK) {
        ESP_LOGI(TAG, "entering setup mode (%s)",
                 requested ? "requested from the status bar"
                 : button  ? "button held"
                 : err == ESP_ERR_NOT_FOUND ? "no configuration"
                                            : "invalid configuration");
        ESP_ERROR_CHECK(setup_mode_start(&setup_callbacks));
        return; /* the setup tasks keep running */
    }

    ui_start(&cfg);
}
