/*
 * Status bar: Wi-Fi, date and time, battery and the setup button. Also keeps
 * the clock: real-time clock at startup, SNTP when online, Home Assistant's
 * time zone.
 */

#include "ui_internal.hpp"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"

static const char *TAG = "status";

namespace {

constexpr uint32_t BATTERY_PERIOD_MS = 60000;
constexpr uint32_t WIFI_PERIOD_MS = 30000;

uint32_t battery_due_ms = 0;
uint32_t wifi_due_ms = 0;
bool sntp_started = false;

int measure_battery()
{
    int sum = 0, n = 0;
    for (int i = 0; i < 16; i++) {
        int mv = M5.Power.getBatteryVoltage();
        if (mv > 0) {
            sum += mv;
            n++;
        }
    }
    return n > 0 ? ui_battery_percent(sum / n) : -1;
}

const char *wifi_icon()
{
    if (!ui.wifi_up) {
        return "wifi-strength-off-outline";
    }
    if (!ui.ha_up) {
        return "wifi-strength-alert-outline";
    }
    static const char *levels[] = {"wifi-strength-1", "wifi-strength-1", "wifi-strength-2",
                                   "wifi-strength-3", "wifi-strength-4"};
    int l = ui.wifi_level < 0 ? 0 : ui.wifi_level > 4 ? 4 : ui.wifi_level;
    return levels[l];
}

void on_sntp_sync(struct timeval *tv)
{
    UiMsg msg = {};
    msg.kind = UiMsgKind::TimeSynced;
    ui_post(msg);
}

}  // namespace

void status_init()
{
    /* The real-time clock keeps UTC; set the system clock from it so the
     * time shows before Wi-Fi is up. */
    setenv("TZ", "UTC0", 1);
    tzset();
    if (M5.Rtc.isEnabled() && !M5.Rtc.getVoltLow()) {
        m5::rtc_datetime_t dt;
        if (M5.Rtc.getDateTime(&dt)) {
            struct tm tm = dt.get_tm();
            if (ui_time_valid(&tm)) {
                struct timeval tv = {.tv_sec = mktime(&tm), .tv_usec = 0};
                settimeofday(&tv, nullptr);
                ESP_LOGI(TAG, "time from the real-time clock");
            }
        }
    } else {
        ESP_LOGW(TAG, "real-time clock has no valid time");
    }
    ui.battery_pct = measure_battery();
}

void status_set_time_zone(const char *iana)
{
    const char *posix = tz_posix_for(iana);
    if (posix == nullptr) {
        ESP_LOGW(TAG, "unknown time zone %s, using UTC", iana);
        posix = "UTC0";
    }
    setenv("TZ", posix, 1);
    tzset();
    ui.shown_minute = -1; /* redraw */
}

void status_on_wifi_up()
{
    if (sntp_started) {
        return;
    }
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = on_sntp_sync;
    if (esp_netif_sntp_init(&cfg) == ESP_OK) {
        sntp_started = true;
    }
}

void status_on_time_synced()
{
    time_t now = time(nullptr);
    struct tm utc;
    gmtime_r(&now, &utc);
    if (M5.Rtc.isEnabled()) {
        M5.Rtc.setDateTime(&utc);
    }
    ESP_LOGI(TAG, "time synchronized");
    ui.shown_minute = -1;
}

void status_draw()
{
    auto &d = M5.Display;
    ui_rect_t bar = rect_status_bar();
    d.fillRect(bar.x, bar.y, bar.w, bar.h, TFT_WHITE);
    d.drawFastHLine(0, bar.h - 2, bar.w, TFT_BLACK);
    d.drawFastHLine(0, bar.h - 1, bar.w, TFT_BLACK);

    draw_icon(wifi_icon(), 32, 12, 7, false);

    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    char text[24];
    ui_format_time(&local, text, sizeof text);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    use_font(28);
    d.setTextDatum(textdatum_t::middle_center);
    d.drawString(text, UI_WIDTH / 2 - 10, bar.h / 2 - 1);

    ui_rect_t gear = rect_gear();
    int bx = gear.x - 8;
    if (ui.battery_pct >= 0) {
        char pct[16];
        snprintf(pct, sizeof pct, "%d%%", ui.battery_pct);
        use_font(20);
        d.setTextDatum(textdatum_t::middle_right);
        d.drawString(pct, bx, bar.h / 2 - 1);
        bx -= d.textWidth(pct) + 4;
        draw_icon(ui_battery_icon(ui.battery_pct), 32, bx - 32, 7, false);
    }
    draw_icon("mdi:cog", 32, gear.x + (gear.w - 32) / 2, 7, false);
}

void status_refresh()
{
    render_region(rect_status_bar(), epd_mode_t::epd_fast);
}

void status_tick(uint32_t now_ms)
{
    bool changed = false;

    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    int minute = ui_time_valid(&local) ? local.tm_hour * 60 + local.tm_min : -2;
    if (minute != ui.shown_minute) {
        ui.shown_minute = minute;
        changed = true;
    }

    if ((int32_t)(now_ms - battery_due_ms) >= 0) {
        battery_due_ms = now_ms + BATTERY_PERIOD_MS;
        int pct = measure_battery();
        if (pct != ui.battery_pct) {
            ui.battery_pct = pct;
            changed = true;
        }
    }

    if ((int32_t)(now_ms - wifi_due_ms) >= 0) {
        wifi_due_ms = now_ms + WIFI_PERIOD_MS;
        int level = ui_wifi_level(ha_wifi_rssi());
        if (level != ui.wifi_level) {
            ui.wifi_level = level;
            changed = true;
        }
    }

    if (changed) {
        status_refresh();
    }
}
