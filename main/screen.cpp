#include "screen.h"

#include <M5Unified.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {

SemaphoreHandle_t lock;

constexpr int MARGIN = 32;

// Draws text horizontally centered at y.
void centered(const char *text, int y, const lgfx::IFont *font)
{
    M5.Display.setFont(font);
    M5.Display.setTextDatum(textdatum_t::top_center);
    M5.Display.drawString(text, M5.Display.width() / 2, y);
}

void left(const char *text, int x, int y, const lgfx::IFont *font)
{
    M5.Display.setFont(font);
    M5.Display.setTextDatum(textdatum_t::top_left);
    M5.Display.drawString(text, x, y);
}

// Starts a full-quality redraw on a white screen.
void begin_screen()
{
    xSemaphoreTake(lock, portMAX_DELAY);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.startWrite();
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
}

void end_screen()
{
    M5.Display.endWrite();
    M5.Display.display();
    M5.Display.waitDisplay();
    xSemaphoreGive(lock);
}

}  // namespace

extern "C" void screen_init(void)
{
    lock = xSemaphoreCreateMutex();

    auto cfg = M5.config();
    cfg.clear_display = false;  // keep the last image until the first screen
    M5.begin(cfg);

    // Portrait: the panel is 960 x 540 in its native orientation.
    if (M5.Display.width() > M5.Display.height()) {
        M5.Display.setRotation(M5.Display.getRotation() ^ 1);
    }
}

extern "C" void screen_show_message(const char *text)
{
    begin_screen();
    centered(text, M5.Display.height() / 2 - 20, &fonts::FreeSansBold18pt7b);
    end_screen();
}

extern "C" void screen_show_setup(const char *ssid, const char *password, const char *url)
{
    char wifi_qr[128];
    snprintf(wifi_qr, sizeof wifi_qr, "WIFI:T:WPA;S:%s;P:%s;;", ssid, password);

    begin_screen();
    const int w = M5.Display.width();
    int y = 48;

    centered("Setup", y, &fonts::FreeSansBold24pt7b);
    y += 88;

    left("1. Join the Wi-Fi network below.", MARGIN, y, &fonts::FreeSans12pt7b);
    y += 40;
    left("2. Open the address below.", MARGIN, y, &fonts::FreeSans12pt7b);
    y += 40;
    left("3. Upload your configuration file.", MARGIN, y, &fonts::FreeSans12pt7b);
    y += 64;

    left("Network", MARGIN, y, &fonts::FreeSans12pt7b);
    left(ssid, MARGIN, y + 32, &fonts::FreeMonoBold18pt7b);
    y += 96;
    left("Password", MARGIN, y, &fonts::FreeSans12pt7b);
    left(password, MARGIN, y + 32, &fonts::FreeMonoBold18pt7b);
    y += 112;

    // Two QR codes side by side: join the network, open the page.
    const int qr = (w - 3 * MARGIN) / 2;
    M5.Display.qrcode(wifi_qr, MARGIN, y, qr);
    M5.Display.qrcode(url, 2 * MARGIN + qr, y, qr);
    y += qr + 12;
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.setTextDatum(textdatum_t::top_center);
    M5.Display.drawString("Scan to join", MARGIN + qr / 2, y);
    M5.Display.drawString("Scan to open", 2 * MARGIN + qr + qr / 2, y);
    y += 64;

    centered(url, y, &fonts::FreeMonoBold18pt7b);
    end_screen();
}

extern "C" void screen_show_configured(const config_t *cfg)
{
    begin_screen();
    int y = 48;
    centered("Configured", y, &fonts::FreeSansBold24pt7b);
    y += 120;
    left("Home Assistant", MARGIN, y, &fonts::FreeSans12pt7b);
    left(cfg->ha_url, MARGIN, y + 32, &fonts::FreeSansBold12pt7b);
    y += 96;
    left("Dashboard", MARGIN, y, &fonts::FreeSans12pt7b);
    left(cfg->ha_dashboard, MARGIN, y + 32, &fonts::FreeSansBold12pt7b);
    y += 128;
    left("The remote control comes in a later version.", MARGIN, y, &fonts::FreeSans12pt7b);
    end_screen();
}
