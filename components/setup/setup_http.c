/*
 * Web server for setup mode: the upload page, the upload endpoint and the
 * captive-portal redirect.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "device_config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "setup_internal.h"

static const char *TAG = "setup_http";

/* page.html, embedded by EMBED_TXTFILES (NUL-terminated). */
extern const char page_html_start[] asm("_binary_page_html_start");
extern const char page_html_end[] asm("_binary_page_html_end");

static const setup_callbacks_t *s_callbacks;
static volatile bool s_restarting;

/* ---- helpers ---- */

static esp_err_t send_json(httpd_req_t *req, const char *status, const cJSON *body)
{
    char *text = cJSON_PrintUnformatted(body);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, text != NULL ? text : "{\"ok\":false}");
    cJSON_free(text);
    return err;
}

static esp_err_t send_errors(httpd_req_t *req, const char *status, const char *const *msgs, int n)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddBoolToObject(body, "ok", false);
    cJSON *list = cJSON_AddArrayToObject(body, "errors");
    for (int i = 0; i < n; i++) {
        cJSON_AddItemToArray(list, cJSON_CreateString(msgs[i]));
    }
    esp_err_t err = send_json(req, status, body);
    cJSON_Delete(body);
    return err;
}

static esp_err_t send_one_error(httpd_req_t *req, const char *status, const char *msg)
{
    return send_errors(req, status, &msg, 1);
}

/* ---- restart ---- */

static void restart_task(void *arg)
{
    /* Give the HTTP response time to leave the device. */
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (s_callbacks != NULL && s_callbacks->on_saved != NULL) {
        s_callbacks->on_saved();
    }
    ESP_LOGI(TAG, "restarting with the new configuration");
    esp_restart();
}

/* ---- handlers ---- */

static esp_err_t page_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, page_html_start, page_html_end - page_html_start - 1);
}

static esp_err_t config_post(httpd_req_t *req)
{
    if (s_restarting) {
        return send_one_error(req, "409 Conflict", "file: a configuration was just saved");
    }
    if (req->content_len > CONFIG_MAX_SIZE) {
        char msg[64];
        snprintf(msg, sizeof msg, "file: larger than %d bytes", CONFIG_MAX_SIZE);
        return send_one_error(req, "413 Payload Too Large", msg);
    }

    size_t len = req->content_len;
    char *body = malloc(len + 1);
    if (body == NULL) {
        return send_one_error(req, "500 Internal Server Error", "file: out of memory");
    }
    size_t got = 0;
    while (got < len) {
        int r = httpd_req_recv(req, body + got, len - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (r <= 0) {
            free(body);
            return ESP_FAIL; /* connection lost: nothing to answer */
        }
        got += r;
    }
    body[len] = '\0';

    config_errors_t errs;
    config_t cfg;
    bool ok = config_validate(body, len, &cfg, &errs);
    config_free(&cfg);

    esp_err_t result;
    if (!ok) {
        ESP_LOGW(TAG, "upload rejected with %d errors", errs.count);
        const char *msgs[CONFIG_MAX_ERRORS];
        int n = errs.count < CONFIG_MAX_ERRORS ? errs.count : CONFIG_MAX_ERRORS;
        for (int i = 0; i < n; i++) {
            msgs[i] = errs.msg[i];
        }
        result = send_errors(req, "400 Bad Request", msgs, n);
    } else if (config_store(body, len) != ESP_OK) {
        result = send_one_error(req, "500 Internal Server Error", "file: could not be stored");
    } else {
        s_restarting = true;
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddBoolToObject(resp, "ok", true);
        result = send_json(req, "200 OK", resp);
        cJSON_Delete(resp);
        xTaskCreate(restart_task, "setup_restart", 4096, NULL, 5, NULL);
    }
    memset(body, 0, len); /* contains the token */
    free(body);
    return result;
}

/* Every unknown path redirects to the upload page. Phones probe URLs like
 * /generate_204 or /hotspot-detect.html and show the page when redirected. */
static esp_err_t redirect(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", SETUP_URL);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, NULL, 0);
}

esp_err_t setup_http_start(const setup_callbacks_t *callbacks)
{
    s_callbacks = callbacks;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192; /* validation parses JSON in this task */
    cfg.lru_purge_enable = true;
    cfg.max_open_sockets = 7;

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start: %s", esp_err_to_name(err));
        return err;
    }
    static const httpd_uri_t page = {.uri = "/", .method = HTTP_GET, .handler = page_get};
    static const httpd_uri_t upload = {.uri = "/config", .method = HTTP_POST, .handler = config_post};
    httpd_register_uri_handler(server, &page);
    httpd_register_uri_handler(server, &upload);
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, redirect);
    ESP_LOGI(TAG, "upload page at %s", SETUP_URL);
    return ESP_OK;
}
