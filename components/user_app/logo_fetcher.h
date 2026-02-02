#ifndef LOGO_FETCHER_H
#define LOGO_FETCHER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LOGO_WIDTH 80
#define MAX_LOGO_HEIGHT 80

// Target logo size for display (ESPN combiner serves 72x72, we scale to 36x36)
#define TARGET_LOGO_SIZE 36

typedef struct {
  uint8_t *data; // RGB565 pixel data (dithered for B/W display)
  int width;
  int height;
  bool valid;
} logo_data_t;

/**
 * @brief Initialize logo fetcher system
 */
void logo_fetcher_init(void);

/**
 * @brief Fetch and decode a logo from URL (with in-memory caching)
 * @param url Logo URL
 * @param out_logo Output logo data (caller must free data buffer when done)
 * @return true if successful, false otherwise
 */
bool logo_fetcher_get(const char *url, logo_data_t *out_logo);

/**
 * @brief Free logo data
 * @param logo Logo to free
 */
void logo_fetcher_free(logo_data_t *logo);

/**
 * @brief Clear in-memory logo cache (call when games data changes)
 */
void logo_fetcher_clear_cache(void);

#ifdef __cplusplus
}
#endif

#endif // LOGO_FETCHER_H
