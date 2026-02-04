# Waveshare RLCD Dashboard

A dashboard project for the Waveshare RLCD 4.2 devboard. The dashboard displays real-time sports scores, local temp/humidity data, and system status markers.
![Image](https://github.com/user-attachments/assets/d8ebd071-0091-411d-88b0-8cd5ce153c46)

## Features

- **Sports Scores**: Real-time scores and game status fetched from the ESPN API.
- **Team Logos**: Automatic fetching and processing of team logos.
  - High-contrast silhouette rendering for light-colored logos to improve visibility on the RLCD.
  - Multi-level caching (In-memory + SD Card) to minimize network usage and improve performance.
- **7-Segment look Clock**: High-visibility time display with a blinking colon, synchronized via NTP.
- **Climate Monitoring**: Real-time temperature (°F) and humidity (%) display using the SHTC3 sensor.
- **System Indicators**:
  - Battery level percentage and visual indicator bar.
  - WiFi signal strength (RSSI) indicator bars.

## Hardware Requirements

- **Waveshare RLCD 4.2 Devboard**: https://docs.waveshare.com/ESP32-S3-RLCD-4.2
- **Storage**: microSD Card for logo caching (connected via 1-bit SD mode).


## Configuration

### WiFi and Security
Create a `secrets.h` file in `components/app_bsp/` (see `secrets.h.example`):
```c
#define WIFI_SSID "your_ssid"
#define WIFI_PASS "your_password"
```

### Sports & League
Configure your preferred sport and league in `components/user_app/sports_config.h`:
```c
#define SPORT_TYPE "basketball"
#define LEAGUE_TYPE "mens-college-basketball"
#define LEAGUE_GROUP "23" // Conference/Group ID
```

## Project Structure

- `components/ui_bsp/`: Waveshare RLCD driver and LVGL porting.
- `components/user_app/`: Main application logic.
  - `dashboard_screen.c`: UI layout and update functions.
  - `logo_fetcher.c`: Logic for downloading, processing, and caching PNG logos.
  - `sports_scores.c`: ESPN API integration and data parsing.
  - `seven_seg.c`: Custom canvas-based 7-segment display logic.
- `main/`: Entry point and application initialization.
