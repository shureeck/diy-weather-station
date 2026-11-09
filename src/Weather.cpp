#include "Weather.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>
#include "ui.h"

// ==== Preferences ====
static const float LATITUDE = 54.52f; 
static const float LONGITUDE = 18.53f;

static const uint32_t WEATHER_PERIOD_MS = 30UL * 60UL * 1000UL; // 30 mins
static const uint32_t WEATHER_RETRY_MS = 5UL * 60UL * 1000UL;   // Retry on error
static const int FORECAST_HOURS = 5;

#define WEATHER_PROB_LABEL ui_RainProbLabel // Label из SquareLine
static const int PROB_THRESHOLD = 20;       // % для показа Rain/Snow без кода осадков
// ====================

enum WeatherState
{
    W_DEFAULT,
    W_RAIN,
    W_SNOW
};

static WeatherState currentState = W_DEFAULT;
static bool firstShown = false;

// Коды WMO: https://open-meteo.com/en/docs
static bool isSnowCode(int c)
{
    return (c >= 71 && c <= 77) || c == 85 || c == 86;
}

static bool isRainCode(int c)
{
    return (c >= 51 && c <= 67) || (c >= 80 && c <= 82) || (c >= 95 && c <= 99);
}

static void showWeather(WeatherState s)
{
    if (firstShown && s == currentState)
        return;

    firstShown = true;
    currentState = s;

    // Сначала скрываем все
    lv_obj_add_flag(ui_Rain,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Snow,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_Default, LV_OBJ_FLAG_HIDDEN);

    // Потом показываем нужную
    switch (s)
    {
    case W_RAIN:
        lv_obj_clear_flag(ui_Rain, LV_OBJ_FLAG_HIDDEN);
        break;
    case W_SNOW:
        lv_obj_clear_flag(ui_Snow, LV_OBJ_FLAG_HIDDEN);
        break;
    default:
        lv_obj_clear_flag(ui_Default, LV_OBJ_FLAG_HIDDEN);
        break;
    }
}

static bool fetchWeather()
{
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(8000);

    char url[300];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&hourly=weather_code,precipitation_probability,temperature_2m"
             "&forecast_hours=%d&timezone=auto",
             LATITUDE, LONGITUDE, FORECAST_HOURS);

    if (!http.begin(client, url))
        return false;

   int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        http.end();
        return false;
    }
    String payload = http.getString();   
    http.end();

    if (payload.length() == 0)
        return false;

    JsonDocument filter;
    filter["hourly"]["weather_code"] = true;
    filter["hourly"]["precipitation_probability"] = true;
    filter["hourly"]["temperature_2m"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, payload, DeserializationOption::Filter(filter));

    if (err)
    {
        return false;
    }

    JsonArray codes = doc["hourly"]["weather_code"];
    JsonArray probs = doc["hourly"]["precipitation_probability"];
    JsonArray temps = doc["hourly"]["temperature_2m"];

    if (codes.isNull())
        return false;

    WeatherState result = W_DEFAULT;
    int maxProb = -1;

    for (size_t i = 0; i < codes.size(); i++)
    {
        int c = codes[i] | 0;
        int p = probs[i] | -1; // null -> -1
        float t = temps[i] | 10.0f;

        if (p > maxProb)
            maxProb = p;

        if (result != W_DEFAULT)
            continue; // тип уже определён по более раннему часу

        if (isSnowCode(c))
            result = W_SNOW;
        else if (isRainCode(c))
            result = W_RAIN;
        else if (p >= PROB_THRESHOLD)
            result = (t <= 0.0f) ? W_SNOW : W_RAIN;
    }

     if (maxProb >= 0)
        lv_label_set_text_fmt(WEATHER_PROB_LABEL, "%d%%", maxProb);
    else
        lv_label_set_text(WEATHER_PROB_LABEL, "err");
    showWeather(result);
    if (result == W_DEFAULT)
        lv_obj_add_flag(WEATHER_PROB_LABEL, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(WEATHER_PROB_LABEL, LV_OBJ_FLAG_HIDDEN);
    return true;
}

void weatherLoop()
{
    static uint32_t lastAttempt = 0;
    static bool lastOk = false;
    static bool started = false;

    if (WiFi.status() != WL_CONNECTED)
        return;

    uint32_t interval = lastOk ? WEATHER_PERIOD_MS : WEATHER_RETRY_MS;

    if (!started || millis() - lastAttempt >= interval)
    {
        started = true;
        lastAttempt = millis();
        lastOk = fetchWeather();
    }
}