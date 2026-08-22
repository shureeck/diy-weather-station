#include "Tuya.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>

#include "ui.h"

static const char *CLIENT_ID = "Feel Tuya ClientID";

static const char *SECRET = "Feel Tuya SecretID";;

static const char *DEVICE_ID = "Feel Inner temperature sensor ID";
static const char *DEVICE_STREET_ID = "Feel Outer temperature sensor ID";

static String accessToken;

static uint32_t lastUpdate = 0;

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
    http.end();
    return accessToken.length() > 0;
}

void getDeviceFromResponse(JsonDocument doc, String deviceId, lv_obj_t *tempLabel, lv_obj_t *humLabel)
{
      JsonArray arr =
        doc["result"][deviceId].as<JsonArray>();
    int temp = 0;
    int hum = 0;
    for (JsonObject item : arr)
    {
        String code = item["code"];
        if (code == "va_temperature")
        {
            int rawTemp = item["value"].as<int>();
            temp = (rawTemp + 5) / 10;
        }
        if (code == "va_humidity")
        {
            int rawHum = item["value"].as<int>();
            hum = (rawHum + 5) / 10;
        }
    }
    lv_label_set_text_fmt(tempLabel, "%d", temp);
    lv_label_set_text_fmt(humLabel, "%d", hum);
}

void readDevice()
{
    getAccessToken();
    HTTPClient http;
    String timestamp =
        String((uint64_t)time(nullptr) * 1000ULL);
    String url =
        "/v1.0/devices/status?device_ids=" +
        String(DEVICE_ID)
        + ","+ String(DEVICE_STREET_ID);
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

    Serial.println("===== DEVICE DEBUG =====");
    Serial.println(timestamp);
    Serial.println(stringToSign);
    Serial.println(sign);

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
    http.end();
}

void tuyaInit()
{
    readDevice();
}

void tuyaLoop()
{
    if (millis() - lastUpdate < 60000)
        return;

    lastUpdate = millis();

    readDevice();
}