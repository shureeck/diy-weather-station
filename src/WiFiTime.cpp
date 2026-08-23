#include "WiFiTime.h"

#include <WiFi.h>
#include <time.h>
#include <lvgl.h>
#include "ui.h"
#include <Preferences.h>

//==================== WIFI ====================

static Preferences preferences;

static String savedSSID;
static String savedPassword;

static bool wifiScanning = false;
static bool wifiScanRequest = false;
static int wifiNetworkCount = 0;

static bool loadWiFiCredentials()
{
    preferences.begin("wifi", true);

    savedSSID = preferences.getString("ssid", "");
    savedPassword = preferences.getString("password", "");
    Serial.println("Load credentials");
    Serial.println(savedSSID);
    Serial.println(savedPassword);

    preferences.end();

    return savedSSID.length() > 0;
}

static void saveWiFiCredentials(
    const char *ssid,
    const char *password)
{
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    savedSSID = ssid;
    savedPassword = password;
    Serial.println("Save wifi creds");
    Serial.println(savedSSID);
    Serial.println(savedPassword);

    // Hide keyboard
    lv_obj_add_flag(ui_Keyboard, LV_OBJ_FLAG_HIDDEN);

    // Hide WiFi preferences panel
    lv_obj_add_flag(ui_WiFiPref, LV_OBJ_FLAG_HIDDEN);

    // Remove keyboard from textarea
    lv_keyboard_set_textarea(ui_Keyboard, NULL);
}

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

    Serial.println("Wifi");

    lv_label_set_text(ui_WiFiLabel, "WiFi...");

    WiFi.mode(WIFI_STA);

    if (loadWiFiCredentials())
    {
        Serial.print("Connecting to saved WiFi: ");
        Serial.println(savedSSID);

        WiFi.begin(
            savedSSID.c_str(),
            savedPassword.c_str());
    }
    else
    {
        Serial.println("No saved WiFi");

        lv_label_set_text(
            ui_WiFiLabel,
            "No WiFi");
    }

    lastNtpSync = millis();
    lastMinuteUpdate = millis();
}

void wifi_start_scan()
{
    if (wifiScanning)
        return;

    wifiScanning = true;

    // Clear old data
    lv_dropdown_clear_options(ui_Network);

    // Text while scan
    lv_dropdown_set_options(ui_Network, "Scanning...");

    WiFi.mode(WIFI_STA);

    // Async scan
    WiFi.scanNetworks(true);
}

void wifi_icon_event_cb(lv_event_t *e)
{
    Serial.println("Click");
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;

    if (lv_obj_has_flag(ui_WiFiPref, LV_OBJ_FLAG_HIDDEN))
    {
        // Keyboard is hidden → show it
        lv_obj_clear_flag(ui_WiFiPref, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        // Keyboard is visible → hide it
        lv_obj_add_flag(ui_WiFiPref, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_Keyboard, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // Запускаем сканирование
    wifiScanRequest = true;
}

void wifi_scan_check()
{
    if (!wifiScanning)
        return;

    int result = WiFi.scanComplete();

    if (result == WIFI_SCAN_RUNNING)
        return;

    wifiScanning = false;

    if (result < 0)
    {
        lv_dropdown_set_options(ui_Network, "Scan failed");
        return;
    }

    lv_dropdown_clear_options(ui_Network);

    bool firstNetwork = true;

    for (int i = 0; i < result; i++)
    {
        String ssid = WiFi.SSID(i);

        if (ssid.length() == 0)
            continue;

        if (firstNetwork)
        {
            lv_dropdown_set_options(
                ui_Network,
                ssid.c_str());

            firstNetwork = false;
        }
        else
        {
            lv_dropdown_add_option(
                ui_Network,
                ssid.c_str(),
                LV_DROPDOWN_POS_LAST);
        }
    }

    if (firstNetwork)
    {
        lv_dropdown_set_options(
            ui_Network,
            "No networks found");
    }

    WiFi.scanDelete();
}

void wifiTimeLoop()
{
    if (wifiScanRequest)
    {
        wifiScanRequest = false;
        wifi_start_scan();
    }
    wifi_scan_check();

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
            loadWiFiCredentials();
            WiFi.begin(savedSSID, savedPassword);
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

void network_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
        return;

    char ssid[64];

    lv_dropdown_get_selected_str(
        ui_Network,
        ssid,
        sizeof(ssid));

    Serial.print("WiFi selected: ");
    Serial.println(ssid);
}

void wifi_password_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;

    // Показываем клавиатуру
    lv_obj_clear_flag(ui_Keyboard, LV_OBJ_FLAG_HIDDEN);

    // Привязываем клавиатуру к TextArea
    lv_keyboard_set_textarea(
        ui_Keyboard,
        ui_WiFiPass);
}

void saveWiFiCredentials()
{
    char ssid[64];

    lv_dropdown_get_selected_str(
        ui_Network,
        ssid,
        sizeof(ssid));

    const char *password = lv_textarea_get_text(ui_WiFiPass);

    saveWiFiCredentials(ssid, password);
    wifiTimeInit();
}

void keyboard_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_READY)
    {
        saveWiFiCredentials();
    }
}
