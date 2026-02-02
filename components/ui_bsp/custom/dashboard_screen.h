#ifndef DASHBOARD_SCREEN_H
#define DASHBOARD_SCREEN_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char away_abbrev[8];
  char home_abbrev[8];
  int away_score;
  int home_score;
  char status[32]; // "FINAL", "2nd 5:30", "1/31 - 7:00 PM"
  char away_logo_url[128];
  char home_logo_url[128];
  bool is_live;
} game_info_t;

void dashboard_create(lv_obj_t *parent);
void dashboard_update_time(int hours, int minutes, bool colon_visible);
void dashboard_update_scores(game_info_t *games, int count);
void dashboard_update_date(const char *date_str);
void dashboard_update_climate(float temp_f, float humidity);
void dashboard_update_battery(uint8_t percent);
void dashboard_update_wifi(int8_t rssi, bool connected);
lv_obj_t *dashboard_get_container(void);

#ifdef __cplusplus
}
#endif

#endif // DASHBOARD_SCREEN_H
