# STICKCPLUS2-THI-AirGate

[🇯🇵 日本語 (Japanese)](README.ja.md)

An intelligent ventilation control system using M5StickCPlus2 that automatically switches an air gate based on the Temperature Humidity Index (THI) difference between two locations.

## Device Photo

![Device photo](images/IMG_8834_resized.jpeg)

## Features

- **Automatic Display Rotation**: Uses IMU (accelerometer) to detect gravity and automatically switches between normal (0°) and flipped (180°) landscape orientations
- **MQTT Data Reception**: Receives THI values from two locations (living room and entrance) via MQTT
- **Smart Gate Control**: Automatically opens/closes an air gate based on THI differential with hysteresis to prevent chattering
- **Burn-in Prevention**: Animated screensaver mode with drifting state text when display is idle
- **Motion-Triggered Wake**: Shake or touch the device to wake from screensaver mode
- **Data Validity Monitoring**: Detects data timeout (30 seconds) and displays OFFLINE status
- **Robust Connection Handling**: Non-blocking WiFi/MQTT connection with timeout and retry limits
- **Fail-Safe Offline Policy**: When data is stale/unavailable, the gate state is forced to CLOSE
- **Self-Recovery**: Automatic reboot after 24 hours of uptime for long-term stability
- **Health Logging**: Periodic health logs include boot count, reconnect counters, and data freshness
- **Dynamic Display Layout**: UI adapts to different screen sizes
- **Detailed Error Logging**: Serial output includes detailed error information (WiFi, MQTT, JSON parsing)

## Hardware Requirements

- **M5StickCPlus2**: Main controller with built-in IMU, WiFi, and TFT display
- **USB-C Power Adapter**: For continuous operation (power supplied externally)
- **MQTT Broker**: Network MQTT server for data exchange

## Software Requirements

- **Arduino IDE** or **PlatformIO**
- **M5StickCPlus2 Board Support Package** (ESP32 3.3.7+)
- **Arduino Libraries**:
  - M5StickCPlus2
  - WiFi
  - PubSubClient
  - ArduinoJson

## Installation

### 1. Clone or Download the Project
```bash
git clone <repository-url>
cd STICKCPLUS2-THI-AirGate
```

### 2. Install Board Support
In Arduino IDE:
- **File** → **Preferences** → **Additional Boards Manager URLs**
- Add: `https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json`
- **Tools** → **Board** → **Boards Manager** → Search "M5" → Install "M5Stack by M5Stack Official"

### 3. Install Required Libraries
**Sketch** → **Include Library** → **Manage Libraries**:
- Search and install: `M5StickCPlus2`, `PubSubClient`, `ArduinoJson`

### 4. Configure Settings
Copy and edit the configuration file:
```bash
cp config.example.h config.h
```

Edit `config.h` with your WiFi and MQTT details:
```cpp
#define WIFI_SSID "Your-WiFi-SSID"
#define WIFI_PASSWORD "Your-WiFi-Password"
#define MQTT_BROKER "192.168.x.x"
#define MQTT_PORT 1883
#define MQTT_TOPIC_LIVING "home/sensor/living"
#define MQTT_TOPIC_ENTRANCE "home/sensor/entrance"
#define THI_OPEN_DIFF 0.5
#define THI_CLOSE_DIFF 0.2
```

### 5. Upload to Device
- Select Board: **Tools** → **Board** → **M5Stack** → **M5Stick-C PLUS2**
- Select Port: **Tools** → **Port** → `/dev/tty.usbserial-xxxxx`
- **Sketch** → **Upload** (or press Ctrl+U)

## Configuration

### MQTT Topics
- **Living Room**: Topic to receive THI from living area
- **Entrance**: Topic to receive THI from entrance

The system expects JSON payloads:
```json
{"thi": 23.5}
```

### THI Hysteresis
- `THI_OPEN_DIFF`: Open gate when `livingTHI - entranceTHI ≥ value`
- `THI_CLOSE_DIFF`: Close gate when `livingTHI - entranceTHI ≤ value`

Default: Open ≥ 0.5, Close ≤ 0.2

### Display Parameters
Edit these parameters (`config.h` and sketch constants):
- `DISPLAY_BRIGHTNESS_NORMAL` (in `config.h`): Normal display brightness (0-255)
- `DISPLAY_SAVER_DIM_PERCENT` (in `config.h`): Screensaver dim percentage (0-100)
- `NORMAL_DISPLAY_HOLD_MS`: Duration to show normal display after motion detection (15000ms = 15s)
- `MOTION_DELTA_THRESHOLD`: Sensitivity for motion/shake detection (0.18 default)
- `DATA_TIMEOUT_MS`: Time before marking data as offline (30000ms = 30s)
- `WIFI_CONNECT_TIMEOUT_MS`: WiFi connection timeout (30000ms = 30s)
- `AUTO_REBOOT_INTERVAL_MS`: Auto-reboot interval for self-recovery (default: 24 hours)
- `HEALTH_LOG_INTERVAL_MS`: Interval of periodic health logs (default: 60s)

## Usage

### Normal Operation
1. Power on the device via USB-C
2. Device attempts WiFi connection (display shows "WiFi...")
3. If successful, attempts MQTT connection (display shows "MQTT...")
4. Once connected, displays current gate state (OPEN/CLOSE) with THI values
5. State updates when data arrives via MQTT

### Display Modes

**Normal Display Mode** (after motion/wake):
- Large state text (OPEN/CLOSE) with color coding
- THI difference (dTHI)
- Living room and entrance THI values
- [OFFLINE] indicator if data is stale

**Screensaver Mode** (idle for 15 seconds):
- Animated "OPEN" or "CLOSE" text that drifts on screen
- Current gate state color (green=OPEN, red=CLOSE)
- "Shake/Touch to wake" prompt

### Automatic Features

- **Display Rotation**: Tilt device to rotate display between normal and 180° modes
- **Motion Wake**: Shake or touch device to wake from screensaver
- **State Color Coding**:
  - Green: OPEN
  - Red: CLOSE
  - Yellow: WAIT (waiting for data)
  - Orange: [OFFLINE] (data timeout)

## Troubleshooting

### WiFi Connection Fails
- Check SSID and password in `config.h`
- Ensure device is in range of WiFi network
- Check WiFi 2.4GHz availability (M5StickCPlus2 doesn't support 5GHz)
- Device will continue operating in screensaver mode even if WiFi fails

### MQTT Connection Fails
- Verify MQTT broker address and port in `config.h`
- Check MQTT broker is running and accessible
- Verify firewall isn't blocking port 1883
- See Serial output for detailed error code

### No Data Updates
- Check MQTT topics in `config.h` match your broker
- Verify sensors are publishing valid JSON with "thi" field
- If no update for 30 seconds, the device enters fail-safe mode (forces CLOSE)
- Display shows [OFFLINE] when data is stale
- Check Serial output for JSON parsing errors

### Display Issues
- **Flickering**: Normal when state changes; should stabilize quickly
- **Rotation not working**: Try tilting device more deliberately
- **Screensaver not animating**: Check `SAVER_FRAME_INTERVAL_MS` setting

### Serial Debugging
Connect via USB and open Serial Monitor (115200 baud) to see:
- WiFi connection status
- MQTT connection attempts and errors
- THI values received
- Data timeout warnings
- JSON parsing errors with details
- Boot count (persistent)
- WiFi/MQTT reconnect counters
- Last-receive timestamps and data age

## Technical Details

### Architecture
- **Continuous Polling**: 10ms main loop for responsive IMU detection
- **Event-Driven Rendering**: Only redraws when state actually changes (prevents flicker)
- **Dual IMU Function**: Gravity detection for rotation + Motion detection for wake
- **Non-Blocking Design**: WiFi and MQTT operations timeout rather than block
- **Fail-Safe Policy**: Offline/stale data condition forces gate state to CLOSE
- **Scheduled Self-Recovery**: Automatic reboot at fixed uptime interval (24h default)

### Key State Variables
- `currentState`: "OPEN", "CLOSE", or "WAIT"
- `dataOffline`: True when data has timed out
- `bootCount`: Persistent boot counter (stored in NVS)
- `mqttReconnectCount` / `wifiReconnectCount`: Reconnect counters for runtime monitoring
- `isNormalDisplayActive()`: Returns true if within wake timeout period
- `gateOpen`: Hysteresis state of air gate

### Update Intervals
- Rotation check: 200ms
- Motion detection: 100ms
- Display refresh: Variable (redraw only on change)
- MQTT reconnect attempts: 5 second intervals, max 5 attempts
- Health log output: every 60 seconds
- Auto reboot: every 24 hours (default)

## File Structure

```
STICKCPLUS2-THI-AirGate/
├── STICKCPLUS2-THI-AirGate.ino  # Main sketch
├── config.h                      # Configuration (keep secret!)
├── config.example.h              # Example configuration template
├── README.md                     # English documentation
├── README.ja.md                  # Japanese documentation
├── .gitignore                    # Git ignore rules
└── .vscode/                      # VS Code settings
```

## Security Notes

- `config.h` contains WiFi and MQTT credentials; **keep it private**
- Add `config.h` to `.gitignore` to avoid accidental commits
- Use `config.example.h` as a template for sharing project
- Consider using WiFi WPA2 security
- Change MQTT broker credentials regularly

## Future Enhancements

- Button controls for manual mode override
- Configuration via web interface
- Data logging to SD card
- Multiple sensor locations
- Adjustable hysteresis via MQTT
- Home Assistant integration

## License

MIT License - See [LICENSE](LICENSE) file for details.

Copyright (c) 2026 omiya-bonsai

## Author

omiya-bonsai

## Development Note

This project was developed with AI-assisted coding support (GitHub Copilot / LLM).
All code, configuration, and behavior were reviewed and validated by the author.

## Support

For issues, questions, or suggestions:
- Check Serial output for detailed logs
- Verify configuration matches your MQTT broker
- Ensure M5StickCPlus2 board support package is up to date
