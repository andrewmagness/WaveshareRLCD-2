#include "esp_wifi_bsp.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "secrets.h"
#include <stdio.h>

#include "string.h"

EventGroupHandle_t wifi_even_ = NULL;

esp_bsp_t user_esp_bsp;

static esp_netif_t *net = NULL;
static const char *TAG = "WiFi";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_MAX_RETRY 10

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

void espwifi_init(void) {
  memset(&user_esp_bsp, 0, sizeof(esp_bsp_t));
  user_esp_bsp.wifi_connected = false;
  wifi_even_ = xEventGroupCreate();
  nvs_flash_init();                          // Initialize default NVS storage
  esp_netif_init();                          // Initialize TCP/IP stack
  esp_event_loop_create_default();           // Create default event loop
  net = esp_netif_create_default_wifi_sta(); // Add TCP/IP stack to the default
                                             // event loop
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // Default configuration
  esp_wifi_init(&cfg);                                 // Initialize WiFi
  esp_event_handler_instance_t Instance_WIFI;
  esp_event_handler_instance_t Instance_IP;
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      &event_handler, NULL, &Instance_WIFI);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                      &event_handler, NULL, &Instance_IP);
  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASSWORD,
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          },
  };
  esp_wifi_set_mode(WIFI_MODE_STA);               // Set mode to STA
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config); // Configure WiFi
  esp_wifi_start();                               // Start WiFi

  ESP_LOGI(TAG, "WiFi initialization complete, connecting to %s",
           wifi_config.sta.ssid);
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
    ESP_LOGI(TAG, "Connecting to AP...");
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    user_esp_bsp.wifi_connected = false;
    if (s_retry_num < WIFI_MAX_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGW(TAG, "Retry connecting to AP (attempt %d/%d)", s_retry_num,
               WIFI_MAX_RETRY);
    } else {
      xEventGroupSetBits(wifi_even_, WIFI_FAIL_BIT);
      ESP_LOGE(TAG, "Failed to connect to AP after %d attempts",
               WIFI_MAX_RETRY);
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    uint32_t pxip = event->ip_info.ip.addr;
    sprintf(user_esp_bsp._ip, "%d.%d.%d.%d", (uint8_t)(pxip),
            (uint8_t)(pxip >> 8), (uint8_t)(pxip >> 16), (uint8_t)(pxip >> 24));
    ESP_LOGI(TAG, "Connected! IP: %s", user_esp_bsp._ip);
    user_esp_bsp.wifi_connected = true;
    s_retry_num = 0;
    xEventGroupSetBits(wifi_even_, WIFI_CONNECTED_BIT);
  }
}

void espwifi_deinit(void) {
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_netif_destroy_default_wifi(net);
  esp_event_loop_delete_default();
}

int8_t espwifi_get_rssi(void) {
  if (!user_esp_bsp.wifi_connected) {
    return -100; // No signal
  }
  wifi_ap_record_t ap_info;
  if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    user_esp_bsp.rssi = ap_info.rssi;
    return ap_info.rssi;
  }
  return -100;
}

bool espwifi_is_connected(void) { return user_esp_bsp.wifi_connected; }
