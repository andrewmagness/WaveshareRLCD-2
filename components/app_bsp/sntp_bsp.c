#include "sntp_bsp.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>


static const char *TAG = "SNTP";
static bool time_synced = false;

static void time_sync_notification_cb(struct timeval *tv) {
  ESP_LOGI(TAG, "Time synchronized with NTP server!");
  time_synced = true;
}

void sntp_time_init(void) {
  ESP_LOGI(TAG, "Initializing SNTP client...");

  // Set timezone to CST (Central Standard Time = UTC-6)
  // CST6CDT means: 6 hours behind UTC, with daylight saving
  setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
  tzset();

  // Configure SNTP
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  esp_sntp_setservername(1, "time.nist.gov");
  esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
  esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

  // Start SNTP
  esp_sntp_init();

  ESP_LOGI(TAG, "SNTP initialized, waiting for time sync...");
}

bool sntp_time_is_synced(void) { return time_synced; }

void sntp_get_time(struct tm *timeinfo) {
  time_t now;
  time(&now);
  localtime_r(&now, timeinfo);
}

void sntp_get_hms(int *hours, int *minutes, int *seconds) {
  struct tm timeinfo;
  sntp_get_time(&timeinfo);

  if (hours)
    *hours = timeinfo.tm_hour;
  if (minutes)
    *minutes = timeinfo.tm_min;
  if (seconds)
    *seconds = timeinfo.tm_sec;
}
