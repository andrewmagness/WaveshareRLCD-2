#ifndef SNTP_BSP_H
#define SNTP_BSP_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SNTP client and set timezone to CST (UTC-6)
 */
void sntp_time_init(void);

/**
 * @brief Check if time has been synchronized with NTP server
 * @return true if synchronized, false otherwise
 */
bool sntp_time_is_synced(void);

/**
 * @brief Get current time
 * @param timeinfo Pointer to struct tm to fill with current time
 */
void sntp_get_time(struct tm *timeinfo);

/**
 * @brief Get current time as hours, minutes, seconds
 * @param hours Pointer to store hours (0-23)
 * @param minutes Pointer to store minutes (0-59)
 * @param seconds Pointer to store seconds (0-59)
 */
void sntp_get_hms(int *hours, int *minutes, int *seconds);

#ifdef __cplusplus
}
#endif

#endif // SNTP_BSP_H
