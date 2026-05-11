#pragma once

// =====================================================
// WIFI
// =====================================================

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// =====================================================
// MQTT
// =====================================================

#define MQTT_BROKER "192.168.3.82"
#define MQTT_PORT 1883

// =====================================================
// MQTT TOPICS
// =====================================================

#define MQTT_TOPIC_LIVING "home/co2/aggregate/raw"
#define MQTT_TOPIC_ENTRANCE "home/env/env4-entrance/raw"

// =====================================================
// THI HYSTERESIS
// =====================================================

// OPEN when:
// livingTHI - entranceTHI >= 0.5

#define THI_OPEN_DIFF 0.5

// CLOSE when:
// livingTHI - entranceTHI <= 0.2

#define THI_CLOSE_DIFF 0.2

// =====================================================
// DISPLAY BRIGHTNESS
// =====================================================

// 0-255 (higher is brighter)
#define DISPLAY_BRIGHTNESS_NORMAL 255

// Dim percentage used in screensaver mode (0-100)
// Example: 30 means 30% dimmer than normal brightness
#define DISPLAY_SAVER_DIM_PERCENT 30