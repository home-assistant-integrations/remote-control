/*
 * Storage of config.json on the LittleFS "config" partition.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "device_config.h"
#include "esp_littlefs.h"
#include "esp_log.h"

static const char *TAG = "config";

#define CONFIG_BASE "/config"
#define CONFIG_FILE CONFIG_BASE "/config.json"
#define CONFIG_TMP  CONFIG_BASE "/config.json.tmp"

esp_err_t config_fs_init(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = CONFIG_BASE,
        .partition_label = "config",
        .format_if_mount_failed = true,
    };
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mounting the config partition failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t config_store(const char *json, size_t len)
{
    FILE *f = fopen(CONFIG_TMP, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "cannot create %s: errno %d", CONFIG_TMP, errno);
        return ESP_FAIL;
    }
    bool ok = fwrite(json, 1, len, f) == len;
    ok = fflush(f) == 0 && ok;
    ok = fsync(fileno(f)) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        ESP_LOGE(TAG, "writing %s failed: errno %d", CONFIG_TMP, errno);
        unlink(CONFIG_TMP);
        return ESP_FAIL;
    }
    /* LittleFS renames atomically and replaces the target, so a power loss
     * leaves either the old or the new file. */
    if (rename(CONFIG_TMP, CONFIG_FILE) != 0) {
        ESP_LOGE(TAG, "replacing %s failed: errno %d", CONFIG_FILE, errno);
        unlink(CONFIG_TMP);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "configuration stored (%u bytes)", (unsigned)len);
    return ESP_OK;
}

esp_err_t config_load(config_t *out)
{
    memset(out, 0, sizeof *out);
    unlink(CONFIG_TMP); /* left over from an interrupted store */

    FILE *f = fopen(CONFIG_FILE, "rb");
    if (f == NULL) {
        ESP_LOGI(TAG, "no configuration stored");
        return ESP_ERR_NOT_FOUND;
    }
    /* Read one byte more than allowed, so an oversized file is detected. */
    char *buf = malloc(CONFIG_MAX_SIZE + 1);
    if (buf == NULL) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    size_t len = fread(buf, 1, CONFIG_MAX_SIZE + 1, f);
    fclose(f);

    config_errors_t errs;
    bool ok = config_validate(buf, len, out, &errs);
    memset(buf, 0, len); /* contains the token */
    free(buf);

    if (!ok) {
        ESP_LOGW(TAG, "stored configuration is invalid (%d errors):", errs.count);
        for (int i = 0; i < errs.count && i < CONFIG_MAX_ERRORS; i++) {
            ESP_LOGW(TAG, "  %s", errs.msg[i]);
        }
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "configuration loaded: url %s, dashboard %s", out->ha_url, out->ha_dashboard);
    return ESP_OK;
}
