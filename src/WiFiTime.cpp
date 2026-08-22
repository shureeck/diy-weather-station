#include "WiFiTime.h"

#include <WiFi.h>
#include <time.h>
#include <lvgl.h>
#include "ui.h"

//==================== WIFI ====================

const char *WIFI_SSID = "Feel WifiName";
const char *WIFI_PASS = "Feel WiFi password";

//==================== TIME ====================

static bool timeReady = false;

static uint32_t lastMinuteUpdate = 0;
static uint32_t lastNtpSync = 0;

extern void updateBacklight(const struct tm &timeinfo);

static void updateTimeLabels()
{
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo, 1000))
        return;

    char buf[16];

    // Время
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    lv_label_set_text(ui_timeLabel, buf);

    // Дата
    strftime(buf, sizeof(buf), "%d.%m", &timeinfo);
    lv_label_set_text(ui_dateLabel, buf);
    updateBacklight(timeinfo);
}

void wifiTimeInit()
{
    lv_label_set_text(ui_WiFiLabel, "WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    uint32_t start = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000)
    {
        delay(200);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        lv_label_set_text(ui_WiFiLabel, "No WiFi");
        return;
    }

    lv_label_set_text(ui_WiFiLabel, WiFi.SSID().c_str());

    // ==========================================
    // ВАРШАВА: CET зимой / CEST летом
    // ==========================================

    configTime(
        7200,
        1,
        "pol.ntp.org");

    struct tm timeinfo;

    if (getLocalTime(&timeinfo, 15000))
    {
        timeReady = true;
        updateTimeLabels();
    }
    else
    {
        timeReady = false;
        Serial.println("NTP TIME FAILED");
    }

    lastNtpSync = millis();
    lastMinuteUpdate = millis();
}
void wifiTimeLoop()
{
    static uint32_t reconnectTimer = 0;
    static bool connectedShown = false;

    if (WiFi.status() != WL_CONNECTED)
    {
        connectedShown = false;
        timeReady = false;

        if (millis() - reconnectTimer >= 10000)
        {
            reconnectTimer = millis();

            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }

        lv_label_set_text(ui_WiFiLabel, "No WiFi");
        return;
    }

    // WiFi подключился
    if (!connectedShown)
    {
        connectedShown = true;

        lv_label_set_text(ui_WiFiLabel, WiFi.SSID().c_str());

        configTime(7200, 1, "pool.ntp.org");

        struct tm timeinfo;

        if (getLocalTime(&timeinfo, 10000))
        {
            timeReady = true;
            updateTimeLabels();
        }

        lastNtpSync = millis();
        lastMinuteUpdate = millis();
    }

    // Обновляем отображение каждую минуту
    if (timeReady && millis() - lastMinuteUpdate >= 60000)
    {
        lastMinuteUpdate = millis();
        updateTimeLabels();
    }

    // NTP раз в 2 часа
    if (millis() - lastNtpSync >= 7200000UL)
    {
        lastNtpSync = millis();

        configTime(7200, 1, "pool.ntp.org");

        struct tm timeinfo;

        if (getLocalTime(&timeinfo, 10000))
        {
            timeReady = true;
            updateTimeLabels();
        }
    }
}