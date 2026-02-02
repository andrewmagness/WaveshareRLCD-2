#include "user_app.h"
#include "adc_bsp.h"
#include "dashboard_screen.h"
#include "esp_wifi_bsp.h"
#include "i2c_bsp.h"
#include "i2c_equipment.h"
#include "logo_fetcher.h"
#include "lvgl_bsp.h"
#include "sdcard_bsp.h"
#include "sntp_bsp.h"
#include "sports_scores.h"
#include <esp_log.h>
#include <freertos/freeRTOS.h>
#include <stdio.h>

static const char *TAG = "UserApp";

// I2C bus and sensors
I2cMasterBus I2cbus(14, 13, 0);
Shtc3Port *shtc3port = NULL;

// Dashboard update task
void Dashboard_UpdateTask(void *arg) {
  uint32_t ticks = 0;

  // Wait for WiFi and NTP sync
  ESP_LOGI(TAG, "Waiting for time sync...");

  for (;;) {
    if (Lvgl_lock(LVGL_TASK_MAX_DELAY_MS)) {
      // ========== UPDATE TIME ==========
      // Update frequently for blinking colon
      {
        int hours, minutes, seconds;

        if (sntp_time_is_synced()) {
          sntp_get_hms(&hours, &minutes, &seconds);
        } else {
          hours = 0;
          minutes = 0;
          seconds = 0;
        }

        // Blink colon: approx 600ms ON, 400ms OFF
        bool colon_visible = (ticks % 5) < 3;
        dashboard_update_time(hours, minutes, colon_visible);
      }

      // ========== UPDATE SCORES (every 1 second) ==========
      if (ticks % 5 == 0) {
        game_info_t games[2];
        int count = sports_scores_get_games(games, 2);
        if (count > 0) {
          dashboard_update_scores(games, count);
        }
      }

      // ========== UPDATE DATE (faster initial, then 1 min) ==========
      bool synced = sntp_time_is_synced();
      static bool last_synced = false;
      if ((synced && !last_synced) || (ticks % 300 == 0)) {
        if (synced) {
          struct tm timeinfo;
          char date_str[32];
          sntp_get_time(&timeinfo);
          strftime(date_str, sizeof(date_str), "%b %d, %Y", &timeinfo);
          dashboard_update_date(date_str);
          last_synced = true;
        }
      }

      // ========== UPDATE BATTERY (every 10 seconds) ==========
      if (ticks % 50 == 0) { // Every 10 seconds
        uint8_t level = Adc_GetBatteryLevel();
        dashboard_update_battery(level);
      }

      // ========== UPDATE WIFI (every 5 seconds) ==========
      if (ticks % 25 == 0) { // Every 5 seconds
        bool connected = espwifi_is_connected();
        int8_t rssi = espwifi_get_rssi();
        dashboard_update_wifi(rssi, connected);

        // Initialize SNTP once WiFi connects
        static bool sntp_started = false;
        if (connected && !sntp_started) {
          sntp_time_init();
          sntp_started = true;
          ESP_LOGI(TAG, "Started SNTP sync");
        }
      }

      // ========== UPDATE CLIMATE (every 5 seconds) ==========
      if (ticks % 25 == 0) { // Every 5 seconds
        float rh, temp_c;
        if (shtc3port->Shtc3_ReadTempHumi(&temp_c, &rh) == 0) {
          // Convert Celsius to Fahrenheit
          float temp_f = temp_c * 9.0f / 5.0f + 32.0f;
          dashboard_update_climate(temp_f, rh);
        }
      }

      Lvgl_unlock();
    }

    vTaskDelay(pdMS_TO_TICKS(200));
    ticks++;
  }
}

void UserApp_AppInit() {
  ESP_LOGI(TAG, "Initializing application...");

  // Initialize ADC for battery monitoring
  Adc_PortInit();

  // Initialize I2C sensor
  shtc3port = new Shtc3Port(I2cbus);

  // Initialize SD card for logo caching
  // GPIO38=CLK, GPIO21=CMD, GPIO39=D0 (1-bit mode)
  static CustomSDPort *sdcard = new CustomSDPort("/sdcard", 38, 21, 39, 1);
  if (sdcard->SDPort_GetStatus()) {
    ESP_LOGI(TAG, "SD card mounted successfully");
  } else {
    ESP_LOGW(TAG, "SD card not available - logos will be fetched each time");
  }

  // Initialize WiFi (will auto-connect to configured AP)
  espwifi_init();

  ESP_LOGI(TAG, "Application initialized");
}

void UserApp_UiInit() {
  ESP_LOGI(TAG, "Creating dashboard UI...");

  // Initialize logo fetcher (PNG decoder) - must be after LVGL init
  logo_fetcher_init();

  // Get the current screen
  lv_obj_t *scr = lv_scr_act();

  // Clear the screen
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // Create the dashboard
  dashboard_create(scr);

  ESP_LOGI(TAG, "Dashboard UI created");
}

void UserApp_TaskInit() {
  ESP_LOGI(TAG, "Starting dashboard update task...");

  // Create the dashboard update task
  xTaskCreatePinnedToCore(Dashboard_UpdateTask, "Dashboard_Task", 8 * 1024,
                          NULL, 2, NULL, 1);

  // Start sports scores task
  sports_scores_init();
}
