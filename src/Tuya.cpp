#include "Tuya.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include "config.h"

#include "ui.h"

static String accessToken;
static uint32_t tokenExpiresAt = 0;
static uint32_t lastUpdate = 0;
static bool firstDone = false;

uint64_t getTimestamp()
{
    return (uint64_t)time(nullptr) * 1000ULL;
}

String hmacSHA256(String key, String msg)
{
    unsigned char hash[32];

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);

    mbedtls_md_setup(&ctx,
                     mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                     1);

    mbedtls_md_hmac_starts(
        &ctx,
        (const unsigned char *)key.c_str(),
        key.length());

    mbedtls_md_hmac_update(
        &ctx,
        (const unsigned char *)msg.c_str(),
        msg.length());

    mbedtls_md_hmac_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    char out[65];

    for (int i = 0; i < 32; i++)
        sprintf(out + i * 2, "%02X", hash[i]);

    return String(out);
}

bool getAccessToken()
{
    HTTPClient http;
    String timestamp = String(getTimestamp());
    String nonce = ""; // empty is allowed for token API
    String stringToSign =
        "GET\n"
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855"
        "\n"
        "\n"
        "/v1.0/token?grant_type=1";
    String signPayload =
        String(CLIENT_ID) +
        timestamp +
        nonce +
        stringToSign;
    String sign =
        hmacSHA256(
            SECRET,
            signPayload);
    /*
        Serial.println("====== TUYA DEBUG ======");
        Serial.println("timestamp:");
        Serial.println(timestamp);

        Serial.println("stringToSign:");
        Serial.println(stringToSign);

        Serial.println("payload:");
        Serial.println(signPayload);

        Serial.println("sign:");
        Serial.println(sign);
    */
    http.begin(
        "https://openapi.tuyaeu.com/v1.0/token?grant_type=1");

    http.addHeader(
        "client_id",
        CLIENT_ID);

    http.addHeader(
        "sign",
        sign);

    http.addHeader(
        "t",
        timestamp);

    http.addHeader(
        "sign_method",
        "HMAC-SHA256");

    int code = http.GET();

    String response = http.getString();
    /*
        Serial.println(response);
    */
    if (code != HTTP_CODE_OK)
    {
        http.end();
        return false;
    }
    JsonDocument doc;
    deserializeJson(
        doc,
        response);
    accessToken =
        doc["result"]["access_token"]
            .as<String>();
    uint32_t expireSec = doc["result"]["expire_time"] | 7200;
    tokenExpiresAt = millis() + (expireSec - 210) * 1000UL;
    http.end();
    return accessToken.length() > 0;
}

bool ensureToken()
{
    if (accessToken.length() > 0 && (int32_t)(millis() - tokenExpiresAt) < 0)
        return true; // token is alive

    return getAccessToken();
}

void getDeviceFromResponse(const JsonDocument &doc, const char *deviceId, lv_obj_t *tempLabel, lv_obj_t *humLabel)
{
    JsonArrayConst arr = doc["result"][deviceId].as<JsonArrayConst>();

    int temp = 0, hum = 0;
    bool hasTemp = false, hasHum = false;

    for (JsonObjectConst item : arr)
    {
        const char *code = item["code"] | "";

        if (strcmp(code, "va_temperature") == 0)
        {
            hasTemp = true;
            temp = lroundf(item["value"].as<int>() / 10.0f);
        }
        else if (strcmp(code, "va_humidity") == 0)
        {
            hasHum = true;
            hum = lroundf(item["value"].as<int>() / 10.0f);
        }
    }

    if (hasTemp)
        lv_label_set_text_fmt(tempLabel, "%d", temp);
    else
        lv_label_set_text(tempLabel, "ER");

    if (hasHum)
        lv_label_set_text_fmt(humLabel, "%d", hum);
    else
        lv_label_set_text(humLabel, "ER");
}

bool readDevice()
{
    if (WiFi.status() != WL_CONNECTED)
        return false;
    if (time(nullptr) < 1700000000)
        return false; // NTP ещё не синхронизирован
    if (!ensureToken())
        return false; // failed to get token
    HTTPClient http;
    String timestamp =
        String((uint64_t)time(nullptr) * 1000ULL);
    String url =
        "/v1.0/devices/status?device_ids=" +
        String(DEVICE_ID) + "," + String(DEVICE_STREET_ID);
    String bodyHash =
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855";
    String stringToSign =
        "GET\n" +
        bodyHash +
        "\n\n" +
        url;

    String signPayload =
        String(CLIENT_ID) +
        accessToken +
        timestamp +
        stringToSign;

    String sign =
        hmacSHA256(
            SECRET,
            signPayload);

    // Serial.println("===== DEVICE DEBUG =====");
    // Serial.println(timestamp);
    // Serial.println(stringToSign);
    // Serial.println(sign);

    http.begin(
        "https://openapi.tuyaeu.com" + url);

    http.addHeader(
        "client_id",
        CLIENT_ID);

    http.addHeader(
        "access_token",
        accessToken);

    http.addHeader(
        "sign",
        sign);

    http.addHeader(
        "t",
        timestamp);

    http.addHeader(
        "sign_method",
        "HMAC-SHA256");

    int code = http.GET();

    String response =
        http.getString();

    Serial.println(response);

    JsonDocument doc;
    deserializeJson(doc, response);
    getDeviceFromResponse(doc, DEVICE_ID, ui_inTemp, ui_inHum);
    getDeviceFromResponse(doc, DEVICE_STREET_ID, ui_outTemp, ui_outHum);
    if (!doc["success"].as<bool>())
    {
        int errCode = doc["code"] | 0;
        // 1010 - token invalid, 1011 - token expired
        if (errCode == 1010 || errCode == 1011)
            accessToken = "";
        http.end();
        return false;
    }

    http.end();
    return true;
}

void tuyaInit()
{
    readDevice();
}

void tuyaLoop()
{
    uint32_t interval = firstDone ? 300000 : 20000; // до первого успеха пробуем каждые 20 с

    if (lastUpdate != 0 && millis() - lastUpdate < interval)
        return;

    lastUpdate = millis();
    if (lastUpdate == 0)
        lastUpdate = 1;

    if (readDevice())
        firstDone = true;
}