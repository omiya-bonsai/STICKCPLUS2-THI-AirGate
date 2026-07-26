#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cmath>

#include "config.h"

WiFiClient espClient;
PubSubClient client(espClient);
Preferences preferences;

// ===== THI VALUES =====
float livingTHI = NAN;
float entranceTHI = NAN;

// ===== DISPLAY STATE =====
String currentState = "BOOT";

// ===== DISPLAY ROTATION (LANDSCAPE ONLY: 1 OR 3) =====
const uint8_t ROTATION_NORMAL = 1;
const uint8_t ROTATION_FLIPPED = 3;
uint8_t currentRotation = ROTATION_NORMAL;

const uint32_t IMU_CHECK_INTERVAL_MS = 200;
const float GRAVITY_SWITCH_THRESHOLD = 0.45f;
const int ROTATION_CONFIRM_COUNT = 3;

const uint32_t MOTION_CHECK_INTERVAL_MS = 100;
const float MOTION_DELTA_THRESHOLD = 0.18f;
const uint32_t NORMAL_DISPLAY_HOLD_MS = 15000;

const uint32_t SAVER_FRAME_INTERVAL_MS = 120;

const uint32_t DATA_TIMEOUT_MS = 30000;
const uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
const int MQTT_RECONNECT_MAX_ATTEMPTS = 5;
const uint32_t HEALTH_LOG_INTERVAL_MS = 60000;
const uint32_t AUTO_REBOOT_INTERVAL_MS = 24UL * 60UL * 60UL * 1000UL;

uint32_t lastImuCheckMs = 0;
uint8_t pendingRotation = ROTATION_NORMAL;
int pendingRotationCount = 0;

uint32_t lastMotionCheckMs = 0;
bool accelInitialized = false;
float lastAx = 0.0f;
float lastAy = 0.0f;
float lastAz = 0.0f;

uint32_t normalDisplayUntilMs = 0;
uint32_t lastSaverFrameMs = 0;
uint8_t normalBrightness = DISPLAY_BRIGHTNESS_NORMAL;
uint8_t saverBrightness = DISPLAY_BRIGHTNESS_NORMAL;
int currentBrightness = -1;

bool needsRedraw = true;

// ===== DATA TIMESTAMPS =====
uint32_t lastLivingReceiveMs = 0;
uint32_t lastEntranceReceiveMs = 0;
uint32_t bootCount = 0;
uint32_t mqttReconnectCount = 0;
uint32_t wifiReconnectCount = 0;
uint32_t appStartMs = 0;
uint32_t lastHealthLogMs = 0;

// ===== MQTT RECEIVE FLAGS =====
bool livingReceived = false;
bool entranceReceived = false;
bool dataOffline = false;

// ===== HYSTERESIS =====
bool gateOpen = false;

uint16_t getStateColor(const String &state);

void logHealthStatus()
{
    uint32_t now = millis();
    uint32_t livingAgeMs = (lastLivingReceiveMs == 0) ? 0 : (now - lastLivingReceiveMs);
    uint32_t entranceAgeMs = (lastEntranceReceiveMs == 0) ? 0 : (now - lastEntranceReceiveMs);

    Serial.print("HEALTH boot=");
    Serial.print(bootCount);
    Serial.print(" wifiReconnect=");
    Serial.print(wifiReconnectCount);
    Serial.print(" mqttReconnect=");
    Serial.print(mqttReconnectCount);
    Serial.print(" livingLastMs=");
    Serial.print(lastLivingReceiveMs);
    Serial.print(" livingAgeMs=");
    Serial.print(livingAgeMs);
    Serial.print(" entranceLastMs=");
    Serial.print(lastEntranceReceiveMs);
    Serial.print(" entranceAgeMs=");
    Serial.print(entranceAgeMs);
    Serial.print(" offline=");
    Serial.println(dataOffline ? "1" : "0");
}

void updateHealthLog()
{
    uint32_t now = millis();
    if ((now - lastHealthLogMs) < HEALTH_LOG_INTERVAL_MS)
    {
        return;
    }

    lastHealthLogMs = now;
    logHealthStatus();
}

void maintainWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    static uint32_t lastWiFiRetryMs = 0;
    uint32_t now = millis();
    if ((now - lastWiFiRetryMs) < 10000)
    {
        return;
    }
    lastWiFiRetryMs = now;

    wifiReconnectCount++;
    Serial.print("WiFi reconnect attempt: ");
    Serial.println(wifiReconnectCount);
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void autoRecoverIfNeeded()
{
    if ((millis() - appStartMs) < AUTO_REBOOT_INTERVAL_MS)
    {
        return;
    }

    Serial.println("Auto reboot: 24h uptime reached");
    delay(200);
    ESP.restart();
}

void initBrightnessSettings()
{
    int dimPercent = DISPLAY_SAVER_DIM_PERCENT;
    if (dimPercent < 0)
    {
        dimPercent = 0;
    }
    if (dimPercent > 100)
    {
        dimPercent = 100;
    }

    normalBrightness = (uint8_t)DISPLAY_BRIGHTNESS_NORMAL;
    saverBrightness = (uint8_t)((normalBrightness * (100 - dimPercent)) / 100);
}

void applyBrightness(bool normalMode)
{
    int targetBrightness = normalMode ? normalBrightness : saverBrightness;
    if (targetBrightness == currentBrightness)
    {
        return;
    }

    M5.Display.setBrightness(targetBrightness);
    currentBrightness = targetBrightness;
}

// =====================================================
// WIFI
// =====================================================

void connectWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(10, 20);
    M5.Display.setTextSize(2);
    M5.Display.println("WiFi...");

    uint32_t wifiStartMs = millis();
    int dotCount = 0;

    while (WiFi.status() != WL_CONNECTED)
    {
        if ((millis() - wifiStartMs) > WIFI_CONNECT_TIMEOUT_MS)
        {
            Serial.println();
            Serial.println("WiFi timeout - proceeding anyway");
            break;
        }
        delay(500);
        Serial.print(".");
        dotCount++;
        if (dotCount % 20 == 0)
        {
            M5.Display.print(".");
        }
    }

    Serial.println();
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected");
        Serial.println(WiFi.localIP());
        return;
    }

    Serial.println("WiFi not connected (startup timeout)");
}

// =====================================================
// DISPLAY
// =====================================================

void drawCenterText(const String &text, uint16_t color)
{
    M5.Display.fillScreen(BLACK);

    M5.Display.setTextColor(color);
    M5.Display.setTextDatum(middle_center);

    int w = M5.Display.width();
    int h = M5.Display.height();
    int centerX = w / 2;
    int centerY = h / 2;

    M5.Display.setTextSize(5);
    M5.Display.drawString(text, centerX, centerY - 20);

    M5.Display.setTextSize(2);

    String diffText = "dTHI: " + String(livingTHI - entranceTHI, 1);
    M5.Display.drawString(diffText, centerX, centerY + 20);

    String line2 =
        "L:" + String(livingTHI, 1) +
        " E:" + String(entranceTHI, 1);
    M5.Display.drawString(line2, centerX, centerY + 40);

    if (dataOffline)
    {
        M5.Display.setTextColor(TFT_ORANGE);
        M5.Display.drawString("[OFFLINE]", centerX, h - 10);
    }
}

void drawScreenSaver()
{
    uint32_t now = millis();
    if ((now - lastSaverFrameMs) < SAVER_FRAME_INTERVAL_MS)
    {
        return;
    }
    lastSaverFrameMs = now;

    int w = M5.Display.width();
    int h = M5.Display.height();

    String saverState = gateOpen ? "OPEN" : "CLOSE";
    uint16_t saverColor = gateOpen ? TFT_GREEN : TFT_RED;

    float phase = now * 0.003f;
    int textOffsetX = (int)(std::sin(phase) * 24.0f);
    int textOffsetY = (int)(std::cos(phase * 0.8f) * 10.0f);

    M5.Display.fillScreen(BLACK);

    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(saverColor);
    M5.Display.setTextSize(5);
    M5.Display.drawString(saverState, (w / 2) + textOffsetX, (h / 2) + textOffsetY);

    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_LIGHTGREY);
    M5.Display.drawString("Shake/Touch to wake", w / 2, h - 12);
}

bool isNormalDisplayActive()
{
    return millis() < normalDisplayUntilMs;
}

void wakeNormalDisplay()
{
    normalDisplayUntilMs = millis() + NORMAL_DISPLAY_HOLD_MS;
    applyBrightness(true);
    drawCenterText(currentState, getStateColor(currentState));
}

void updateMotionWakeByImu()
{
    uint32_t now = millis();
    if ((now - lastMotionCheckMs) < MOTION_CHECK_INTERVAL_MS)
    {
        return;
    }
    lastMotionCheckMs = now;

    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    M5.Imu.getAccelData(&ax, &ay, &az);

    if (!accelInitialized)
    {
        lastAx = ax;
        lastAy = ay;
        lastAz = az;
        accelInitialized = true;
        return;
    }

    float dx = ax - lastAx;
    float dy = ay - lastAy;
    float dz = az - lastAz;
    float delta = std::sqrt(dx * dx + dy * dy + dz * dz);

    lastAx = ax;
    lastAy = ay;
    lastAz = az;

    if (delta >= MOTION_DELTA_THRESHOLD)
    {
        bool wasInSaver = !isNormalDisplayActive();
        wakeNormalDisplay();
        if (wasInSaver)
        {
            needsRedraw = true;
        }
    }
}

uint16_t getStateColor(const String &state)
{
    if (state == "OPEN")
    {
        return TFT_GREEN;
    }
    if (state == "CLOSE")
    {
        return TFT_RED;
    }
    if (state == "WAIT")
    {
        return TFT_YELLOW;
    }
    return TFT_WHITE;
}

void updateRotationByGravity()
{
    uint32_t now = millis();
    if ((now - lastImuCheckMs) < IMU_CHECK_INTERVAL_MS)
    {
        return;
    }
    lastImuCheckMs = now;

    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    M5.Imu.getAccelData(&ax, &ay, &az);

    // Only switch between 0 and 180 degree landscape rotations.
    if (std::fabs(ax) < GRAVITY_SWITCH_THRESHOLD)
    {
        pendingRotationCount = 0;
        return;
    }

    uint8_t desiredRotation = (ax >= 0.0f) ? ROTATION_NORMAL : ROTATION_FLIPPED;

    if (desiredRotation == currentRotation)
    {
        pendingRotationCount = 0;
        pendingRotation = desiredRotation;
        return;
    }

    if (desiredRotation != pendingRotation)
    {
        pendingRotation = desiredRotation;
        pendingRotationCount = 1;
        return;
    }

    pendingRotationCount++;
    if (pendingRotationCount < ROTATION_CONFIRM_COUNT)
    {
        return;
    }

    currentRotation = desiredRotation;
    M5.Display.setRotation(currentRotation);
    lastSaverFrameMs = 0;
    needsRedraw = true;

    pendingRotationCount = 0;
}

// =====================================================
// STATE LOGIC
// =====================================================

void updateGateState()
{
    String prevState = currentState;
    uint32_t now = millis();

    // ==========================================
    // DATA TIMEOUT CHECK
    // ==========================================
    bool livingValid = livingReceived && ((now - lastLivingReceiveMs) < DATA_TIMEOUT_MS);
    bool entranceValid = entranceReceived && ((now - lastEntranceReceiveMs) < DATA_TIMEOUT_MS);

    if (!livingValid || !entranceValid)
    {
        if (!dataOffline)
        {
            dataOffline = true;
            needsRedraw = true;
            Serial.println("Data timeout - entering OFFLINE mode");
        }

        // Fail-safe: force gate to CLOSE when data is stale/unavailable.
        gateOpen = false;
        currentState = "CLOSE";
    }
    else
    {
        if (dataOffline)
        {
            dataOffline = false;
            needsRedraw = true;
            Serial.println("Data recovered - exiting OFFLINE mode");
        }

        float diff = livingTHI - entranceTHI;

        // ==========================================
        // HYSTERESIS
        // ==========================================

        // OPEN CONDITION
        if (!gateOpen && diff >= THI_OPEN_DIFF)
        {
            gateOpen = true;
        }

        // CLOSE CONDITION
        if (gateOpen && diff <= THI_CLOSE_DIFF)
        {
            gateOpen = false;
        }

        // ==========================================
        // STATE UPDATE
        // ==========================================

        if (gateOpen)
        {
            currentState = "OPEN";
        }
        else
        {
            currentState = "CLOSE";
        }
    }

    if (currentState != prevState)
    {
        needsRedraw = true;
    }
}

// =====================================================
// MQTT CALLBACK
// =====================================================

void mqttCallback(char *topic, byte *payload, unsigned int length)
{

    StaticJsonDocument<1024> doc;

    DeserializationError error =
        deserializeJson(doc, payload, length);

    if (error)
    {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        return;
    }

    String topicStr = String(topic);
    uint32_t now = millis();

    // ==========================================
    // LIVING ROOM
    // ==========================================

    if (topicStr == MQTT_TOPIC_LIVING)
    {

        if (doc.containsKey("thi"))
        {
            livingTHI = doc["thi"].as<float>();
            livingReceived = true;
            lastLivingReceiveMs = now;
            dataOffline = false;

            Serial.print("Living THI: ");
            Serial.println(livingTHI);
        }
    }

    // ==========================================
    // ENTRANCE
    // ==========================================

    if (topicStr == MQTT_TOPIC_ENTRANCE)
    {

        if (doc.containsKey("thi"))
        {
            entranceTHI = doc["thi"].as<float>();
            entranceReceived = true;
            lastEntranceReceiveMs = now;
            dataOffline = false;

            Serial.print("Entrance THI: ");
            Serial.println(entranceTHI);
        }
    }

    updateGateState();
}

// =====================================================
// MQTT
// =====================================================

void reconnectMQTT()
{
    if (client.connected())
    {
        return;
    }

    static uint32_t lastMqttAttemptMs = 0;
    static int mqttFailCount = 0;

    uint32_t now = millis();
    if ((now - lastMqttAttemptMs) < 5000)
    {
        return;
    }
    lastMqttAttemptMs = now;

    if (mqttFailCount >= MQTT_RECONNECT_MAX_ATTEMPTS)
    {
        Serial.println("MQTT: max reconnection attempts reached");
        return;
    }

    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(10, 20);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_CYAN);
    M5.Display.print("MQTT... (attempt ");
    M5.Display.print(mqttFailCount + 1);
    M5.Display.println("/" + String(MQTT_RECONNECT_MAX_ATTEMPTS) + ")");

    String clientId =
        "STICKCPLUS2-AirGate-" +
        String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD))
    {
        Serial.println("MQTT connected");
        mqttReconnectCount++;
        client.subscribe(MQTT_TOPIC_LIVING);
        client.subscribe(MQTT_TOPIC_ENTRANCE);
        M5.Display.println("MQTT OK");
        mqttFailCount = 0;
        delay(500);
    }
    else
    {
        mqttFailCount++;
        Serial.print("MQTT failed (state: ");
        Serial.print(client.state());
        Serial.println(")");
        M5.Display.print("Failed (state: ");
        M5.Display.print(client.state());
        M5.Display.println(")");
    }
}

// =====================================================
// SETUP
// =====================================================

void setup()
{

    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);

    preferences.begin("airgate", false);
    bootCount = preferences.getUInt("bootCount", 0) + 1;
    preferences.putUInt("bootCount", bootCount);
    appStartMs = millis();

    initBrightnessSettings();

    M5.Display.setRotation(currentRotation);
    applyBrightness(false);

    drawScreenSaver();

    connectWiFi();

    client.setServer(MQTT_BROKER, MQTT_PORT);
    client.setCallback(mqttCallback);

    logHealthStatus();
    needsRedraw = true;
}

// =====================================================
// LOOP
// =====================================================

void loop()
{

    M5.update();

    maintainWiFi();

    reconnectMQTT();

    client.loop();

    bool wasNormalDisplay = isNormalDisplayActive();

    updateMotionWakeByImu();
    updateRotationByGravity();

    uint32_t now = millis();
    bool livingValid = livingReceived && ((now - lastLivingReceiveMs) < DATA_TIMEOUT_MS);
    bool entranceValid = entranceReceived && ((now - lastEntranceReceiveMs) < DATA_TIMEOUT_MS);
    if ((livingValid != true || entranceValid != true) && !dataOffline)
    {
        updateGateState();
    }

    if (isNormalDisplayActive() != wasNormalDisplay)
    {
        needsRedraw = true;
    }

    if (needsRedraw)
    {
        if (isNormalDisplayActive())
        {
            applyBrightness(true);
            drawCenterText(currentState, getStateColor(currentState));
        }
        else
        {
            applyBrightness(false);
            drawScreenSaver();
        }
        needsRedraw = false;
    }

    updateHealthLog();
    autoRecoverIfNeeded();

    delay(10);
}
