#include "seven_seg.h"
#include <stdio.h>

// Segment definitions for digits 0-9
// Segments are: top, top-right, bottom-right, bottom, bottom-left, top-left,
// middle
//               a      b           c           d        e           f        g
static const uint8_t SEGMENT_MAP[10] = {
    0b1111110, // 0: a,b,c,d,e,f
    0b0110000, // 1: b,c
    0b1101101, // 2: a,b,d,e,g
    0b1111001, // 3: a,b,c,d,g
    0b0110011, // 4: b,c,f,g
    0b1011011, // 5: a,c,d,f,g
    0b1011111, // 6: a,c,d,e,f,g
    0b1110000, // 7: a,b,c
    0b1111111, // 8: all segments
    0b1111011, // 9: a,b,c,d,f,g
};

// Medium digit dimensions to fit better
#define SEG_DIGIT_W 32
#define SEG_DIGIT_H 60
#define SEG_THICK 6
#define SEG_GAP 2       // Increased gap to prevent segments from merging
#define DIGIT_SPACING 8 // More even spacing
#define COLON_W 12      // Wider colon for better spacing

// 4 digits + 1 colon + 4 spacing gaps + some padding
#define CANVAS_WIDTH (4 * SEG_DIGIT_W + 1 * COLON_W + 4 * DIGIT_SPACING + 10)
#define CANVAS_HEIGHT (SEG_DIGIT_H + 8)

static lv_obj_t *time_canvas = NULL;
static lv_color_t *canvas_buf = NULL;

// Draw a horizontal segment with pointed ends (like real 7-segment)
// Shape: pointed on left and right ends
static void draw_h_segment(lv_obj_t *canvas, int x, int y, int len,
                           lv_color_t color) {
  int half_thick = SEG_THICK / 2;

  for (int row = 0; row < SEG_THICK; row++) {
    // Calculate how far from center this row is
    int dist_from_center =
        (row < half_thick) ? (half_thick - row) : (row - half_thick + 1);

    // Start and end points taper based on distance from center
    int start_x = x + dist_from_center;
    int end_x = x + len - dist_from_center;

    for (int col = start_x; col <= end_x; col++) {
      lv_canvas_set_px_color(canvas, col, y + row, color);
    }
  }
}

// Draw a vertical segment with pointed ends (like real 7-segment)
// Shape: pointed on top and bottom ends
static void draw_v_segment(lv_obj_t *canvas, int x, int y, int len,
                           lv_color_t color) {
  int half_thick = SEG_THICK / 2;

  for (int col = 0; col < SEG_THICK; col++) {
    // Calculate how far from center this column is
    int dist_from_center =
        (col < half_thick) ? (half_thick - col) : (col - half_thick + 1);

    // Start and end points taper based on distance from center
    int start_y = y + dist_from_center;
    int end_y = y + len - dist_from_center;

    for (int row = start_y; row <= end_y; row++) {
      lv_canvas_set_px_color(canvas, x + col, row, color);
    }
  }
}

// Draw a single 7-segment digit with pointed segment ends
static void draw_digit(lv_obj_t *canvas, int digit, int x_offset,
                       lv_color_t color) {
  if (digit < 0 || digit > 9)
    return;

  uint8_t segments = SEGMENT_MAP[digit];

  int h_seg_len = SEG_DIGIT_W - 2 * SEG_GAP;
  int v_seg_len = (SEG_DIGIT_H - SEG_THICK) / 2 - SEG_GAP;
  int mid_y = SEG_DIGIT_H / 2;

  // Segment a (top horizontal)
  if (segments & 0b1000000) {
    draw_h_segment(canvas, x_offset + SEG_GAP, 0, h_seg_len, color);
  }

  // Segment b (top-right vertical)
  if (segments & 0b0100000) {
    draw_v_segment(canvas, x_offset + SEG_DIGIT_W - SEG_THICK,
                   SEG_THICK / 2 + SEG_GAP, v_seg_len, color);
  }

  // Segment c (bottom-right vertical)
  if (segments & 0b0010000) {
    draw_v_segment(canvas, x_offset + SEG_DIGIT_W - SEG_THICK, mid_y + SEG_GAP,
                   v_seg_len, color);
  }

  // Segment d (bottom horizontal)
  if (segments & 0b0001000) {
    draw_h_segment(canvas, x_offset + SEG_GAP, SEG_DIGIT_H - SEG_THICK,
                   h_seg_len, color);
  }

  // Segment e (bottom-left vertical)
  if (segments & 0b0000100) {
    draw_v_segment(canvas, x_offset, mid_y + SEG_GAP, v_seg_len, color);
  }

  // Segment f (top-left vertical)
  if (segments & 0b0000010) {
    draw_v_segment(canvas, x_offset, SEG_THICK / 2 + SEG_GAP, v_seg_len, color);
  }

  // Segment g (middle horizontal)
  if (segments & 0b0000001) {
    draw_h_segment(canvas, x_offset + SEG_GAP, mid_y - SEG_THICK / 2, h_seg_len,
                   color);
  }
}

static void draw_colon(lv_obj_t *canvas, int x_offset, lv_color_t color) {
  int dot_size = 8; // Larger dots for better visibility and proportion
  int cx = x_offset + COLON_W / 2 - dot_size / 2;

  // Top dot
  int top_y = SEG_DIGIT_H / 3 - dot_size / 2;
  for (int dy = 0; dy < dot_size; dy++) {
    for (int dx = 0; dx < dot_size; dx++) {
      lv_canvas_set_px_color(canvas, cx + dx, top_y + dy, color);
    }
  }

  // Bottom dot
  int bot_y = 2 * SEG_DIGIT_H / 3 - dot_size / 2;
  for (int dy = 0; dy < dot_size; dy++) {
    for (int dx = 0; dx < dot_size; dx++) {
      lv_canvas_set_px_color(canvas, cx + dx, bot_y + dy, color);
    }
  }
}

lv_obj_t *seven_seg_create(lv_obj_t *parent) {
  // Create container
  lv_obj_t *container = lv_obj_create(parent);
  lv_obj_remove_style_all(container);
  lv_obj_set_size(container, CANVAS_WIDTH, CANVAS_HEIGHT);
  lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);

  // Allocate canvas buffer
  canvas_buf = (lv_color_t *)lv_mem_alloc(CANVAS_WIDTH * CANVAS_HEIGHT *
                                          sizeof(lv_color_t));
  if (!canvas_buf) {
    return container;
  }

  // Create canvas for drawing
  time_canvas = lv_canvas_create(container);
  lv_canvas_set_buffer(time_canvas, canvas_buf, CANVAS_WIDTH, CANVAS_HEIGHT,
                       LV_IMG_CF_TRUE_COLOR);
  lv_obj_align(time_canvas, LV_ALIGN_TOP_LEFT, 0, 0);

  // Fill with white background
  lv_canvas_fill_bg(time_canvas, lv_color_white(), LV_OPA_COVER);

  return container;
}

void seven_seg_set_time(lv_obj_t *container, int hours, int minutes,
                        bool colon_visible) {
  if (!time_canvas)
    return;

  static int last_h = -1;
  static int last_m = -1;
  static int last_c = -1;

  // Only redraw if anything changed
  if (hours == last_h && minutes == last_m && (int)colon_visible == last_c) {
    return;
  }

  last_h = hours;
  last_m = minutes;
  last_c = (int)colon_visible;

  // Clear canvas with white
  lv_canvas_fill_bg(time_canvas, lv_color_white(), LV_OPA_COVER);

  lv_color_t seg_color = lv_color_black();
  int x = 0;

  // Hours tens
  draw_digit(time_canvas, hours / 10, x, seg_color);
  x += SEG_DIGIT_W + DIGIT_SPACING;

  // Hours ones
  draw_digit(time_canvas, hours % 10, x, seg_color);
  x += SEG_DIGIT_W + DIGIT_SPACING;

  // Colon (blinking - only draw when visible)
  if (colon_visible) {
    draw_colon(time_canvas, x, seg_color);
  }
  x += COLON_W + DIGIT_SPACING;

  // Minutes tens
  draw_digit(time_canvas, minutes / 10, x, seg_color);
  x += SEG_DIGIT_W + DIGIT_SPACING;

  // Minutes ones
  draw_digit(time_canvas, minutes % 10, x, seg_color);

  lv_obj_invalidate(time_canvas);
}
