#include "sports_scores.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_wifi_bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sntp_bsp.h"
#include "sports_config.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SportsScores";

#define MAX_GAMES 20
#define REFRESH_INTERVAL_MS 60000

static game_info_t games_cache[MAX_GAMES];
static int games_count = 0;
static SemaphoreHandle_t games_mutex = NULL;

static void parse_scores_json(const char *json_string) {
  cJSON *root = cJSON_Parse(json_string);
  if (root == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      ESP_LOGE(TAG, "Failed to parse JSON before: %s", error_ptr);
    } else {
      ESP_LOGE(TAG, "Failed to parse JSON (unknown error)");
    }
    return;
  }

  cJSON *events = cJSON_GetObjectItem(root, "events");
  if (!cJSON_IsArray(events)) {
    cJSON_Delete(root);
    return;
  }

  int array_size = cJSON_GetArraySize(events);
  game_info_t temp_games[MAX_GAMES];
  int temp_count = 0;

  for (int i = 0; i < array_size && temp_count < MAX_GAMES; i++) {
    cJSON *event = cJSON_GetArrayItem(events, i);
    cJSON *competitions = cJSON_GetObjectItem(event, "competitions");
    if (!cJSON_IsArray(competitions))
      continue;
    cJSON *competition = cJSON_GetArrayItem(competitions, 0);

    cJSON *competitors = cJSON_GetObjectItem(competition, "competitors");
    if (!cJSON_IsArray(competitors))
      continue;

    // Get status
    cJSON *status_obj = cJSON_GetObjectItem(event, "status");
    cJSON *type_obj = cJSON_GetObjectItem(status_obj, "type");
    char status_str[32] = "N/A";
    bool is_active = false;

    if (type_obj) {
      cJSON *state = cJSON_GetObjectItem(type_obj, "state");
      if (cJSON_IsString(state) && strcmp(state->valuestring, "in") == 0) {
        is_active = true;
      }

      // For scheduled games, extract just the time and convert to CST
      if (cJSON_IsString(state) && strcmp(state->valuestring, "pre") == 0) {
        cJSON *short_detail_item = cJSON_GetObjectItem(type_obj, "shortDetail");
        if (cJSON_IsString(short_detail_item)) {
          // Format is like "2/3 - 7:00 PM EST"
          // Extract date and time, convert EST to CST (subtract 1 hour)
          const char *detail = short_detail_item->valuestring;
          const char *dash = strstr(detail, " - ");
          if (dash) {
            // Extract date portion (before " - ")
            char date_part[8] = {0};
            int date_len = dash - detail;
            if (date_len > 0 && date_len < 8) {
              strncpy(date_part, detail, date_len);
            }

            int hr = 0, min = 0;
            char ampm[3] = {0};
            if (sscanf(dash + 3, "%d:%d %2s", &hr, &min, ampm) == 3) {
              // Convert EST to CST (subtract 1 hour)
              hr--;
              if (hr == 0)
                hr = 12; // 1:00 PM EST -> 12:00 PM CST
              else if (hr < 0) {
                hr = 11;
                ampm[0] = (ampm[0] == 'P') ? 'A' : 'P';
              } // Midnight wrap
              snprintf(status_str, sizeof(status_str), "%s %d:%02d%c",
                       date_part, hr, min, ampm[0]);
            } else {
              strncpy(status_str, dash + 3, sizeof(status_str) - 1);
            }
          } else {
            strncpy(status_str, short_detail_item->valuestring,
                    sizeof(status_str) - 1);
          }
        }
      } else {
        // For live/final games, use shortDetail as-is (shows score or "Final")
        cJSON *short_detail_item = cJSON_GetObjectItem(type_obj, "shortDetail");
        if (cJSON_IsString(short_detail_item)) {
          strncpy(status_str, short_detail_item->valuestring,
                  sizeof(status_str) - 1);
        }
      }
    }

    // Competitors (usually Home is index 0, Away index 1 or vice versa based on
    // homeAway) We just take first two
    cJSON *comp1 = cJSON_GetArrayItem(competitors, 0);
    cJSON *comp2 = cJSON_GetArrayItem(competitors, 1);

    if (!comp1 || !comp2)
      continue;

    cJSON *team1 = cJSON_GetObjectItem(comp1, "team");
    cJSON *team2 = cJSON_GetObjectItem(comp2, "team");

    // Determine home/away
    cJSON *homeAway1 = cJSON_GetObjectItem(comp1, "homeAway");
    bool c1_is_home =
        (homeAway1 && strcmp(homeAway1->valuestring, "home") == 0);

    cJSON *home_comp = c1_is_home ? comp1 : comp2;
    cJSON *away_comp = c1_is_home ? comp2 : comp1;
    cJSON *home_team = c1_is_home ? team1 : team2;
    cJSON *away_team = c1_is_home ? team2 : team1;

    // Extract names and scores
    char home_abbrev[8] = "HOME";
    char away_abbrev[8] = "AWAY";
    int home_score_val = 0;
    int away_score_val = 0;

    cJSON *ha = cJSON_GetObjectItem(home_team, "abbreviation");
    if (cJSON_IsString(ha))
      strncpy(home_abbrev, ha->valuestring, 7);

    cJSON *aa = cJSON_GetObjectItem(away_team, "abbreviation");
    if (cJSON_IsString(aa))
      strncpy(away_abbrev, aa->valuestring, 7);

    cJSON *hs = cJSON_GetObjectItem(home_comp, "score");
    if (cJSON_IsString(hs))
      home_score_val = atoi(hs->valuestring);

    cJSON *as = cJSON_GetObjectItem(away_comp, "score");
    if (cJSON_IsString(as))
      away_score_val = atoi(as->valuestring);

    // Save to temp
    game_info_t *g = &temp_games[temp_count];
    strncpy(g->home_abbrev, home_abbrev, 7);
    strncpy(g->away_abbrev, away_abbrev, 7);
    g->home_score = home_score_val;
    g->away_score = away_score_val;
    strncpy(g->status, status_str, 31);

    // Extract logos
    cJSON *home_logo = cJSON_GetObjectItem(home_team, "logo");
    if (cJSON_IsString(home_logo)) {
      strncpy(g->home_logo_url, home_logo->valuestring,
              sizeof(g->home_logo_url) - 1);
    } else {
      g->home_logo_url[0] = 0;
    }

    cJSON *away_logo = cJSON_GetObjectItem(away_team, "logo");
    if (cJSON_IsString(away_logo)) {
      strncpy(g->away_logo_url, away_logo->valuestring,
              sizeof(g->away_logo_url) - 1);
    } else {
      g->away_logo_url[0] = 0;
    }

    g->is_live = is_active;
    temp_count++;
  }
  cJSON_Delete(root);

  // Filter/Prioritize logic
  // 1. If any live games, keep only live
  // 2. Else if any Final, keep only Final
  // 3. Else keep all (Scheduled)

  bool any_live = false;
  bool any_final = false;

  for (int i = 0; i < temp_count; i++) {
    if (temp_games[i].is_live)
      any_live = true;
    if (strstr(temp_games[i].status, "Final"))
      any_final = true;
  }

  if (games_mutex) {
    xSemaphoreTake(games_mutex, portMAX_DELAY);
    games_count = 0;
    for (int i = 0; i < temp_count; i++) {
      bool include = false;
      if (any_live) {
        if (temp_games[i].is_live)
          include = true;
      } else if (any_final) {
        if (strstr(temp_games[i].status, "Final"))
          include = true;
      } else {
        include = true; // Show scheduled
      }

      if (include && games_count < MAX_GAMES) {
        games_cache[games_count++] = temp_games[i];
      }
    }
    xSemaphoreGive(games_mutex);
  }
}

static void http_test_task(void *pvParameters) {
  char *response_buffer = NULL;
  int response_len = 0;
  int buffer_cap = 0;

  // Build ESPN API URLdynamically from configuration
  char espn_api_url[256];
  snprintf(espn_api_url, sizeof(espn_api_url),
           "https://site.api.espn.com/apis/site/v2/sports/%s/%s/"
           "scoreboard?groups=%s&limit=20",
           SPORT_TYPE, LEAGUE_TYPE, LEAGUE_GROUP);

  esp_http_client_config_t config = {
      .url = espn_api_url,
      .event_handler = NULL,
      .timeout_ms = 10000,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };

  while (1) {
    // Wait for WiFi and SNTP sync before fetching
    if (!espwifi_is_connected() || !sntp_time_is_synced()) {
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    ESP_LOGI(TAG, "Fetching scores...");
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_open(client, 0);

    if (err == ESP_OK) {
      int content_length = esp_http_client_fetch_headers(client);
      if (content_length < 0) {
        // Chunked encoding
        content_length = 32 * 1024; // Default guess
      }

      // Alloc buffer in PSRAM
      if (!response_buffer) {
        buffer_cap = 192 * 1024; // Use 192KB in PSRAM to be absolutely sure
        response_buffer = heap_caps_malloc(buffer_cap + 1, MALLOC_CAP_SPIRAM);
      }
      response_len = 0;

      if (response_buffer) {
        int read_len;
        while (1) {
          read_len =
              esp_http_client_read(client, response_buffer + response_len,
                                   buffer_cap - response_len);
          if (read_len <= 0) {
            if (read_len < 0)
              ESP_LOGE(TAG, "Read error");
            break;
          }
          response_len += read_len;

          if (response_len >= buffer_cap) {
            ESP_LOGW(TAG, "Buffer full, truncating...");
            break;
          }
        }
        response_buffer[response_len] = 0; // Null terminate
        ESP_LOGI(TAG, "Read %d bytes", response_len);

        // Parse loaded JSON
        parse_scores_json(response_buffer);
      }
    } else {
      ESP_LOGE(TAG, "HTTP connection failed: %d", err);
    }

    esp_http_client_cleanup(client);
    vTaskDelay(pdMS_TO_TICKS(REFRESH_INTERVAL_MS));
  }

  if (response_buffer)
    free(response_buffer);
  vTaskDelete(NULL);
}

void sports_scores_init(void) {
  games_mutex = xSemaphoreCreateMutex();
  xTaskCreate(http_test_task, "curr_games_task", 20480, NULL, 5, NULL);
}

int sports_scores_get_games(game_info_t *games, int max_to_get) {
  if (!games_mutex || games_count == 0 || max_to_get <= 0)
    return 0;

  // Rotate every 15 seconds for two games
  int base_index =
      (xTaskGetTickCount() * portTICK_PERIOD_MS / 15000) % games_count;

  xSemaphoreTake(games_mutex, portMAX_DELAY);
  int count = 0;
  for (int i = 0; i < max_to_get && i < games_count; i++) {
    int idx = (base_index + i) % games_count;
    games[i] = games_cache[idx];
    count++;
  }
  xSemaphoreGive(games_mutex);
  return count;
}
