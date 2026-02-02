#ifndef SEVEN_SEG_H
#define SEVEN_SEG_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a 7-segment time display widget
 * @param parent Parent object to add the display to
 * @return Container object for the time display
 */
lv_obj_t *seven_seg_create(lv_obj_t *parent);

/**
 * @brief Update the time shown on the 7-segment display (HH:MM format)
 * @param container The container returned from seven_seg_create
 * @param hours Hours (0-23)
 * @param minutes Minutes (0-59)
 * @param colon_visible Whether the colon should be visible (for blinking
 * effect)
 */
void seven_seg_set_time(lv_obj_t *container, int hours, int minutes,
                        bool colon_visible);

#ifdef __cplusplus
}
#endif

#endif // SEVEN_SEG_H
