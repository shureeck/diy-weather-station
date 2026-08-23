#pragma once
#include <lvgl.h>

void wifiTimeInit();
void wifiTimeLoop();
void wifi_icon_event_cb(lv_event_t *e);
void network_event_cb(lv_event_t *e);
void wifi_password_event_cb(lv_event_t *e);
void keyboard_event_cb(lv_event_t *e);
void wifi_scan_check();