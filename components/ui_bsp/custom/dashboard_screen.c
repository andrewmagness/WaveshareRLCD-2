#include "dashboard_screen.h"
#include "logo_fetcher.h"
#include "seven_seg.h"
#include <stdio.h>

// Declare external fonts from gui_guider.h
LV_FONT_DECLARE(lv_font_MISANSMEDIUM_20)
LV_FONT_DECLARE(lv_font_MISANSMEDIUM_25)
LV_FONT_DECLARE(lv_font_MISANSMEDIUM_18)

// Dashboard objects
static lv_obj_t *dashboard_cont = NULL;
static lv_obj_t *time_display = NULL;
static lv_obj_t *temp_label = NULL;
static lv_obj_t *humidity_label = NULL;
static lv_obj_t *battery_bar = NULL;
static lv_obj_t *battery_pct_label = NULL;
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *score_label = NULL;
static lv_obj_t *score_label_2 = NULL;
static lv_obj_t *date_label = NULL;

// Logo image objects for two games (away/home for each)
static lv_obj_t *logo_img_g1_away = NULL;
static lv_obj_t *logo_img_g1_home = NULL;
static lv_obj_t *logo_img_g2_away = NULL;
static lv_obj_t *logo_img_g2_home = NULL;

// Logo image descriptors
static lv_img_dsc_t logo_dsc_g1_away;
static lv_img_dsc_t logo_dsc_g1_home;
static lv_img_dsc_t logo_dsc_g2_away;
static lv_img_dsc_t logo_dsc_g2_home;

// Screen dimensions
#define SCREEN_W 400
#define SCREEN_H 300

// Win3.1 style dimensions
#define TITLE_BAR_H 22
#define BORDER_W 3

// Battery icon dimensions
#define BATT_W 24
#define BATT_H 10
#define BATT_TIP_W 2
#define BATT_TIP_H 4

// WiFi bar dimensions
#define WIFI_W 20
#define WIFI_H 14

// Create battery icon for title bar (smaller, white on black)
static lv_obj_t *create_battery_icon(lv_obj_t *parent) {
  lv_obj_t *batt_cont = lv_obj_create(parent);
  lv_obj_remove_style_all(batt_cont);
  lv_obj_set_size(batt_cont, BATT_W + BATT_TIP_W + 2, BATT_H + 2);
  lv_obj_clear_flag(batt_cont, LV_OBJ_FLAG_SCROLLABLE);

  // Battery outline (white)
  lv_obj_t *outline = lv_obj_create(batt_cont);
  lv_obj_remove_style_all(outline);
  lv_obj_set_size(outline, BATT_W, BATT_H);
  lv_obj_set_pos(outline, 0, 1);
  lv_obj_set_style_border_width(outline, 1, 0);
  lv_obj_set_style_border_color(outline, lv_color_white(), 0);
  lv_obj_set_style_radius(outline, 1, 0);
  lv_obj_set_style_bg_opa(outline, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(outline, LV_OBJ_FLAG_SCROLLABLE);

  // Battery tip (white)
  lv_obj_t *tip = lv_obj_create(batt_cont);
  lv_obj_remove_style_all(tip);
  lv_obj_set_size(tip, BATT_TIP_W, BATT_TIP_H);
  lv_obj_set_pos(tip, BATT_W, (BATT_H - BATT_TIP_H) / 2 + 1);
  lv_obj_set_style_bg_color(tip, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(tip, LV_OPA_COVER, 0);
  lv_obj_clear_flag(tip, LV_OBJ_FLAG_SCROLLABLE);

  // Battery fill bar (white, inside)
  battery_bar = lv_obj_create(outline);
  lv_obj_remove_style_all(battery_bar);
  lv_obj_set_size(battery_bar, BATT_W - 4, BATT_H - 4);
  lv_obj_set_pos(battery_bar, 1, 1);
  lv_obj_set_style_bg_color(battery_bar, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(battery_bar, LV_OPA_COVER, 0);
  lv_obj_clear_flag(battery_bar, LV_OBJ_FLAG_SCROLLABLE);

  return batt_cont;
}

// Create WiFi signal bars (white for title bar)
static lv_obj_t *create_wifi_icon(lv_obj_t *parent) {
  lv_obj_t *wifi_cont = lv_obj_create(parent);
  lv_obj_remove_style_all(wifi_cont);
  lv_obj_set_size(wifi_cont, WIFI_W, WIFI_H);
  lv_obj_clear_flag(wifi_cont, LV_OBJ_FLAG_SCROLLABLE);

  int bar_w = 3;
  int gap = 2;
  int heights[] = {3, 6, 9, 12};

  for (int i = 0; i < 4; i++) {
    lv_obj_t *bar = lv_obj_create(wifi_cont);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, bar_w, heights[i]);
    lv_obj_set_pos(bar, i * (bar_w + gap), WIFI_H - heights[i]);
    lv_obj_set_style_bg_color(bar, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  }

  wifi_icon = wifi_cont;
  return wifi_cont;
}

void dashboard_create(lv_obj_t *parent) {
  // ========== MAIN WINDOW CONTAINER ==========
  dashboard_cont = lv_obj_create(parent);
  lv_obj_set_pos(dashboard_cont, 0, 0);
  lv_obj_set_size(dashboard_cont, SCREEN_W, SCREEN_H);
  lv_obj_set_scrollbar_mode(dashboard_cont, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_color(dashboard_cont, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(dashboard_cont, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dashboard_cont, 0, 0);
  lv_obj_set_style_pad_all(dashboard_cont, 0, 0);
  lv_obj_clear_flag(dashboard_cont, LV_OBJ_FLAG_SCROLLABLE);

  // ========== WIN3.1 OUTER BORDER (raised 3D effect) ==========
  // Outer white line (top-left highlight)
  lv_obj_t *border_outer = lv_obj_create(dashboard_cont);
  lv_obj_remove_style_all(border_outer);
  lv_obj_set_pos(border_outer, 0, 0);
  lv_obj_set_size(border_outer, SCREEN_W, SCREEN_H);
  lv_obj_set_style_border_width(border_outer, 2, 0);
  lv_obj_set_style_border_color(border_outer, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(border_outer, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(border_outer, LV_OBJ_FLAG_SCROLLABLE);

  // Inner frame border
  lv_obj_t *border_inner = lv_obj_create(dashboard_cont);
  lv_obj_remove_style_all(border_inner);
  lv_obj_set_pos(border_inner, 3, 3);
  lv_obj_set_size(border_inner, SCREEN_W - 6, SCREEN_H - 6);
  lv_obj_set_style_border_width(border_inner, 1, 0);
  lv_obj_set_style_border_color(border_inner, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(border_inner, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(border_inner, LV_OBJ_FLAG_SCROLLABLE);

  // ========== TITLE BAR (Win3.1 style - black background) ==========
  lv_obj_t *title_bar = lv_obj_create(dashboard_cont);
  lv_obj_remove_style_all(title_bar);
  lv_obj_set_pos(title_bar, 4, 4);
  lv_obj_set_size(title_bar, SCREEN_W - 8, TITLE_BAR_H);
  lv_obj_set_style_bg_color(title_bar, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
  lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

  // Title text - centered, white on black
  lv_obj_t *title_label = lv_label_create(title_bar);
  lv_label_set_text(title_label, "RLCD");
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
  lv_obj_center(title_label);

  // Battery icon in title bar (right side)
  lv_obj_t *batt_icon = create_battery_icon(title_bar);
  lv_obj_set_pos(batt_icon, SCREEN_W - 80, 4);

  // Battery percentage label (in title bar)
  battery_pct_label = lv_label_create(title_bar);
  lv_label_set_text(battery_pct_label, "100%");
  lv_obj_set_pos(battery_pct_label, SCREEN_W - 80, 4);
  lv_obj_align(battery_pct_label, LV_ALIGN_LEFT_MID, SCREEN_W - 120, 0);
  lv_obj_set_style_text_font(battery_pct_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_pct_label, lv_color_white(), 0);

  // WiFi icon in title bar (far right)
  lv_obj_t *wifi = create_wifi_icon(title_bar);
  lv_obj_align(wifi, LV_ALIGN_RIGHT_MID, -8, 0);

  // ========== CONTENT AREA ==========
  int content_y = 4 + TITLE_BAR_H + 4; // Below title bar

  // ========== TIME DISPLAY (TOP LEFT) ==========
  time_display = seven_seg_create(dashboard_cont);
  lv_obj_set_pos(time_display, 15, content_y + 10);

  // Date label just to the right of CST
  lv_obj_t *time_label = lv_label_create(dashboard_cont);
  date_label = lv_label_create(dashboard_cont);
  lv_label_set_text(date_label, "Jan 01, 2026");
  lv_obj_set_pos(date_label, 65, content_y + 95); // Moved up to avoid divider
  lv_obj_set_style_text_font(date_label, &lv_font_MISANSMEDIUM_18, 0);
  lv_obj_set_style_text_color(date_label, lv_color_black(), 0);

  // Adjust CST label position
  lv_label_set_text(time_label, "CST");
  lv_obj_set_pos(time_label, 15, content_y + 95); // Moved up to avoid divider
  lv_obj_set_style_text_font(time_label, &lv_font_MISANSMEDIUM_18, 0);
  lv_obj_set_style_text_color(time_label, lv_color_black(), 0);

  // ========== CLIMATE DISPLAY (LEFT SIDE, BELOW TIME) ==========
  // Temperature section header
  lv_obj_t *temp_text = lv_label_create(dashboard_cont);
  lv_label_set_text(temp_text, "TEMP");
  lv_obj_set_pos(temp_text, 15, content_y + 125); // Moved up
  lv_obj_set_style_text_font(temp_text, &lv_font_MISANSMEDIUM_18, 0);
  lv_obj_set_style_text_color(temp_text, lv_color_black(), 0);

  // Temperature label (large)
  temp_label = lv_label_create(dashboard_cont);
  lv_label_set_text(temp_label, "--.- F");
  lv_obj_set_pos(temp_label, 15, content_y + 145); // Moved up
  lv_obj_set_style_text_font(temp_label, &lv_font_MISANSMEDIUM_25, 0);
  lv_obj_set_style_text_color(temp_label, lv_color_black(), 0);

  // Humidity section header
  lv_obj_t *humid_text = lv_label_create(dashboard_cont);
  lv_label_set_text(humid_text, "HUMIDITY");
  lv_obj_set_pos(humid_text, 15, content_y + 185); // Moved up
  lv_obj_set_style_text_font(humid_text, &lv_font_MISANSMEDIUM_18, 0);
  lv_obj_set_style_text_color(humid_text, lv_color_black(), 0);

  // Humidity label (large)
  humidity_label = lv_label_create(dashboard_cont);
  lv_label_set_text(humidity_label, "--%");
  lv_obj_set_pos(humidity_label, 15, content_y + 205); // Moved up
  lv_obj_set_style_text_font(humidity_label, &lv_font_MISANSMEDIUM_25, 0);
  lv_obj_set_style_text_color(humidity_label, lv_color_black(), 0);

  // ========== WIN3.1 STYLE SEPARATOR LINES ==========
  // Horizontal line below time area
  lv_obj_t *h_line = lv_obj_create(dashboard_cont);
  lv_obj_remove_style_all(h_line);
  lv_obj_set_pos(h_line, 10, content_y + 118); // Moved up
  lv_obj_set_size(h_line, 190, 2);
  lv_obj_set_style_bg_color(h_line, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(h_line, LV_OPA_COVER, 0);
  lv_obj_clear_flag(h_line, LV_OBJ_FLAG_SCROLLABLE);

  // Vertical divider moved to center
  lv_obj_t *v_line = lv_obj_create(dashboard_cont);
  lv_obj_remove_style_all(v_line);
  lv_obj_set_pos(v_line, 205, content_y + 5);
  lv_obj_set_size(v_line, 2, SCREEN_H - content_y - 15);
  lv_obj_set_style_bg_color(v_line, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(v_line, LV_OPA_COVER, 0);
  lv_obj_clear_flag(v_line, LV_OBJ_FLAG_SCROLLABLE);

  // Initialize with placeholder values
  seven_seg_set_time(time_display, 0, 0, true);

  // ========== SCORE DISPLAY (RIGHT SIDE) ==========
  // Right side starts at X=210 (after vertical divider at 205)
  int right_x = 212;
  int logo_size = 35; // Smaller to fit both teams stacked

  // Game 1 section - top half of right side
  int game1_y = content_y + 8;

  // Away team logo (top)
  logo_img_g1_away = lv_img_create(dashboard_cont);
  lv_obj_set_size(logo_img_g1_away, logo_size, logo_size);
  lv_obj_set_pos(logo_img_g1_away, right_x + 3, game1_y);
  lv_obj_add_flag(logo_img_g1_away, LV_OBJ_FLAG_HIDDEN);

  // Home team logo (bottom, below away)
  logo_img_g1_home = lv_img_create(dashboard_cont);
  lv_obj_set_size(logo_img_g1_home, logo_size, logo_size);
  lv_obj_set_pos(logo_img_g1_home, right_x + 3, game1_y + logo_size + 14);
  lv_obj_add_flag(logo_img_g1_home, LV_OBJ_FLAG_HIDDEN);

  // Score and info to the right of logos
  score_label = lv_label_create(dashboard_cont);
  lv_label_set_text(score_label, "Loading...");
  lv_obj_set_pos(score_label, right_x + logo_size + 10, game1_y + 5);
  lv_obj_set_width(score_label, SCREEN_W - right_x - logo_size - 18);
  lv_label_set_long_mode(score_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(score_label, &lv_font_MISANSMEDIUM_20, 0);
  lv_obj_set_style_text_color(score_label, lv_color_black(), 0);
  lv_obj_set_style_text_align(score_label, LV_TEXT_ALIGN_LEFT, 0);

  // Horizontal divider between games
  lv_obj_t *game_divider = lv_obj_create(dashboard_cont);
  lv_obj_remove_style_all(game_divider);
  lv_obj_set_pos(game_divider, right_x, content_y + 106);
  lv_obj_set_size(game_divider, SCREEN_W - right_x - 10, 1);
  lv_obj_set_style_bg_color(game_divider, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(game_divider, LV_OPA_COVER, 0);
  lv_obj_clear_flag(game_divider, LV_OBJ_FLAG_SCROLLABLE);

  // Game 2 section - bottom half of right side
  int game2_y = content_y + 113;

  // Away team logo for game 2
  logo_img_g2_away = lv_img_create(dashboard_cont);
  lv_obj_set_size(logo_img_g2_away, logo_size, logo_size);
  lv_obj_set_pos(logo_img_g2_away, right_x + 3, game2_y);
  lv_obj_add_flag(logo_img_g2_away, LV_OBJ_FLAG_HIDDEN);

  // Home team logo for game 2
  logo_img_g2_home = lv_img_create(dashboard_cont);
  lv_obj_set_size(logo_img_g2_home, logo_size, logo_size);
  lv_obj_set_pos(logo_img_g2_home, right_x + 3, game2_y + logo_size + 14);
  lv_obj_add_flag(logo_img_g2_home, LV_OBJ_FLAG_HIDDEN);

  // Score and info for game 2
  score_label_2 = lv_label_create(dashboard_cont);
  lv_label_set_text(score_label_2, "");
  lv_obj_set_pos(score_label_2, right_x + logo_size + 10, game2_y + 5);
  lv_obj_set_width(score_label_2, SCREEN_W - right_x - logo_size - 18);
  lv_label_set_long_mode(score_label_2, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(score_label_2, &lv_font_MISANSMEDIUM_20, 0);
  lv_obj_set_style_text_color(score_label_2, lv_color_black(), 0);
  lv_obj_set_style_text_align(score_label_2, LV_TEXT_ALIGN_LEFT, 0);
}

void dashboard_update_date(const char *date_str) {
  if (date_label) {
    lv_label_set_text(date_label, date_str);
  }
}

void dashboard_update_time(int hours, int minutes, bool colon_visible) {
  if (time_display) {
    seven_seg_set_time(time_display, hours, minutes, colon_visible);
  }
}

// Helper function to load logo and set on image widget
static void load_and_set_logo(lv_obj_t *img_obj, lv_img_dsc_t *img_dsc,
                              const char *url) {
  if (!img_obj || !img_dsc || !url || strlen(url) == 0) {
    if (img_obj)
      lv_obj_add_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  // Free old logo data if it exists (prevent memory leak)
  if (img_dsc->data) {
    free((void *)img_dsc->data);
    img_dsc->data = NULL;
  }

  logo_data_t logo;
  if (logo_fetcher_get(url, &logo)) {
    // Setup LVGL image descriptor
    img_dsc->header.cf = LV_IMG_CF_TRUE_COLOR;
    img_dsc->header.w = logo.width;
    img_dsc->header.h = logo.height;
    img_dsc->data = logo.data;
    img_dsc->data_size = logo.width * logo.height * 2;

    // Set the image source and show
    lv_img_set_src(img_obj, img_dsc);
    lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(img_obj, LV_OBJ_FLAG_HIDDEN);
  }
}

void dashboard_update_scores(game_info_t *games, int count) {
  if (score_label && count > 0) {
    // Format: Team1 Score\\n  vs\\nTeam2 Score\\nStatus
    // Added leading spaces to "vs" to center it
    lv_label_set_text_fmt(score_label, "%s %d\n   vs\n%s %d\n%s",
                          games[0].away_abbrev, games[0].away_score,
                          games[0].home_abbrev, games[0].home_score,
                          games[0].status);

    // Load and set logos for game 1
    load_and_set_logo(logo_img_g1_away, &logo_dsc_g1_away,
                      games[0].away_logo_url);
    load_and_set_logo(logo_img_g1_home, &logo_dsc_g1_home,
                      games[0].home_logo_url);
  } else if (score_label) {
    lv_label_set_text(score_label, "No games");
    if (logo_img_g1_away)
      lv_obj_add_flag(logo_img_g1_away, LV_OBJ_FLAG_HIDDEN);
    if (logo_img_g1_home)
      lv_obj_add_flag(logo_img_g1_home, LV_OBJ_FLAG_HIDDEN);
  }

  if (score_label_2 && count > 1) {
    lv_label_set_text_fmt(score_label_2, "%s %d\n   vs\n%s %d\n%s",
                          games[1].away_abbrev, games[1].away_score,
                          games[1].home_abbrev, games[1].home_score,
                          games[1].status);

    // Load and set logos for game 2
    load_and_set_logo(logo_img_g2_away, &logo_dsc_g2_away,
                      games[1].away_logo_url);
    load_and_set_logo(logo_img_g2_home, &logo_dsc_g2_home,
                      games[1].home_logo_url);
  } else if (score_label_2) {
    lv_label_set_text(score_label_2, "");
    if (logo_img_g2_away)
      lv_obj_add_flag(logo_img_g2_away, LV_OBJ_FLAG_HIDDEN);
    if (logo_img_g2_home)
      lv_obj_add_flag(logo_img_g2_home, LV_OBJ_FLAG_HIDDEN);
  }
}

void dashboard_update_climate(float temp_f, float humidity) {
  char buf[16];

  if (temp_label) {
    // Format: XX.X F (degree symbol may not be in font, use 'o' as fallback)
    snprintf(buf, sizeof(buf), "%.1f F", temp_f);
    lv_label_set_text(temp_label, buf);
  }

  if (humidity_label) {
    snprintf(buf, sizeof(buf), "%.0f%%", humidity);
    lv_label_set_text(humidity_label, buf);
  }
}

void dashboard_update_battery(uint8_t percent) {
  char buf[8];

  if (percent > 100)
    percent = 100;

  // Update percentage label
  if (battery_pct_label) {
    snprintf(buf, sizeof(buf), "%d%%", percent);
    lv_label_set_text(battery_pct_label, buf);
  }

  // Update fill bar width (max width is BATT_W - 4 = 20)
  if (battery_bar) {
    int fill_w = (BATT_W - 4) * percent / 100;
    if (fill_w < 2)
      fill_w = 2;
    lv_obj_set_width(battery_bar, fill_w);
  }
}

void dashboard_update_wifi(int8_t rssi, bool connected) {
  if (!wifi_icon)
    return;

  // Get number of bars to show based on RSSI
  int bars = 0;
  if (connected) {
    if (rssi > -50)
      bars = 4;
    else if (rssi > -60)
      bars = 3;
    else if (rssi > -70)
      bars = 2;
    else
      bars = 1;
  }

  // Update bar visibility (white = active, dim = inactive)
  int child_count = lv_obj_get_child_cnt(wifi_icon);
  for (int i = 0; i < child_count && i < 4; i++) {
    lv_obj_t *bar = lv_obj_get_child(wifi_icon, i);
    if (i < bars) {
      lv_obj_set_style_bg_color(bar, lv_color_white(), 0);
      lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    } else {
      // Dim for inactive bars
      lv_obj_set_style_bg_opa(bar, LV_OPA_40, 0);
    }
  }
}

lv_obj_t *dashboard_get_container(void) { return dashboard_cont; }
