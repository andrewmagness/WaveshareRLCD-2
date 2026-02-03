#include "logo_fetcher.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "lvgl.h"
#include <string.h>
#include <sys/stat.h>

// Provide custom allocators for LodePNG that use SPIRAM
// These must be defined BEFORE including lodepng.h
void *lodepng_malloc(size_t size) {
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
}

void *lodepng_realloc(void *ptr, size_t new_size) {
  return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
}

void lodepng_free(void *ptr) { free(ptr); }

// Include LVGL's lodepng for PNG decoding
#if LV_USE_PNG
#include "src/extra/libs/png/lodepng.h"
#endif

static const char *TAG = "LogoFetcher";

// SD card logo cache directory
#define LOGO_CACHE_DIR "/sdcard/logos"

// In-memory cache for loaded logos (avoid repeated SD reads)
#define MAX_CACHED_LOGOS 8
typedef struct {
  char team_id[16];
  uint8_t *data;
  int width;
  int height;
  bool valid;
} cached_logo_t;

static cached_logo_t logo_cache[MAX_CACHED_LOGOS];
static int cache_count = 0;

// Flag to track if SD card is available
static bool sd_available = false;

void logo_fetcher_init(void) {
#if LV_USE_PNG
  // Initialize LVGL's PNG decoder
  lv_png_init();
#endif

  // Initialize in-memory cache
  memset(logo_cache, 0, sizeof(logo_cache));
  cache_count = 0;

  // Check if SD card is mounted and create logos directory
  struct stat st;
  if (stat("/sdcard", &st) == 0) {
    sd_available = true;
    ESP_LOGI(TAG, "SD card detected");

    // Create logos cache directory if it doesn't exist
    if (stat(LOGO_CACHE_DIR, &st) != 0) {
      if (mkdir(LOGO_CACHE_DIR, 0755) == 0) {
        ESP_LOGI(TAG, "Created logo cache directory: %s", LOGO_CACHE_DIR);
      } else {
        ESP_LOGW(TAG, "Failed to create cache directory");
      }
    }
  } else {
    ESP_LOGW(TAG, "SD card not available - logos will be fetched each time");
  }

  ESP_LOGI(TAG, "Logo fetcher initialized (SD cache: %s)",
           sd_available ? "enabled" : "disabled");
}

// Extract team ID from ESPN logo URL
// URL format: https://a.espncdn.com/i/teamlogos/ncaa/500/145.png
// Or combiner:
// https://a.espncdn.com/combiner/i?img=/i/teamlogos/ncaa/500/145.png&...
static bool extract_team_id(const char *url, char *team_id, size_t max_len) {
  // Find last '/' before '.png'
  const char *png = strstr(url, ".png");
  if (!png)
    return false;

  const char *last_slash = NULL;
  for (const char *p = url; p < png; p++) {
    if (*p == '/')
      last_slash = p;
  }

  if (!last_slash)
    return false;

  size_t id_len = png - last_slash - 1;
  if (id_len >= max_len || id_len == 0)
    return false;

  strncpy(team_id, last_slash + 1, id_len);
  team_id[id_len] = '\0';
  return true;
}

// Build ESPN combiner URL for properly scaled images
// Transforms: https://a.espncdn.com/i/teamlogos/ncaa/500/145.png
// To:
// https://a.espncdn.com/combiner/i?img=/i/teamlogos/ncaa/500/145.png&w=72&h=72&transparent=true
static void build_combiner_url(const char *original_url, char *combiner_url,
                               size_t max_len) {
  // Look for the path after espncdn.com
  const char *cdn = strstr(original_url, "espncdn.com");
  if (!cdn) {
    // Fallback to original URL
    strncpy(combiner_url, original_url, max_len - 1);
    combiner_url[max_len - 1] = '\0';
    return;
  }

  // Find the path starting with /i/
  const char *path = strstr(cdn, "/i/");
  if (!path) {
    strncpy(combiner_url, original_url, max_len - 1);
    combiner_url[max_len - 1] = '\0';
    return;
  }

  // Build new URL with combiner
  snprintf(combiner_url, max_len,
           "https://a.espncdn.com/combiner/i?img=%s&w=72&h=72&transparent=true",
           path);
}

// Check in-memory cache for a logo
static cached_logo_t *find_cached_logo(const char *team_id) {
  for (int i = 0; i < cache_count; i++) {
    if (strcmp(logo_cache[i].team_id, team_id) == 0 && logo_cache[i].valid) {
      return &logo_cache[i];
    }
  }
  return NULL;
}

// Add logo to in-memory cache
static void add_to_cache(const char *team_id, uint8_t *data, int width,
                         int height) {
  if (cache_count >= MAX_CACHED_LOGOS) {
    // Cache full, evict oldest
    if (logo_cache[0].data) {
      free(logo_cache[0].data);
    }
    memmove(&logo_cache[0], &logo_cache[1],
            (MAX_CACHED_LOGOS - 1) * sizeof(cached_logo_t));
    cache_count = MAX_CACHED_LOGOS - 1;
  }

  // Make a copy of the data for cache
  size_t data_size = width * height * sizeof(uint16_t);
  uint8_t *cache_data = heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM);
  if (cache_data) {
    memcpy(cache_data, data, data_size);
    strncpy(logo_cache[cache_count].team_id, team_id, 15);
    logo_cache[cache_count].team_id[15] = '\0';
    logo_cache[cache_count].data = cache_data;
    logo_cache[cache_count].width = width;
    logo_cache[cache_count].height = height;
    logo_cache[cache_count].valid = true;
    cache_count++;
  }
}

// Build cache file path for a team ID
static void build_cache_path(const char *team_id, char *path, size_t max_len) {
  snprintf(path, max_len, "%s/%s_70.rgb", LOGO_CACHE_DIR, team_id);
}

// Try to load cached logo from SD card
static bool load_sd_cached_logo(const char *team_id, logo_data_t *out_logo) {
  if (!sd_available)
    return false;

  char cache_path[64];
  build_cache_path(team_id, cache_path, sizeof(cache_path));

  FILE *f = fopen(cache_path, "rb");
  if (!f)
    return false;

  // Read header: width (2 bytes) + height (2 bytes)
  uint16_t width, height;
  if (fread(&width, 2, 1, f) != 1 || fread(&height, 2, 1, f) != 1) {
    fclose(f);
    return false;
  }

  // Validate dimensions
  if (width > MAX_LOGO_WIDTH || height > MAX_LOGO_HEIGHT || width == 0 ||
      height == 0) {
    fclose(f);
    return false;
  }

  // Allocate and read RGB565 data
  size_t data_size = width * height * sizeof(uint16_t);
  uint16_t *data = heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM);
  if (!data) {
    fclose(f);
    return false;
  }

  if (fread(data, 1, data_size, f) != data_size) {
    free(data);
    fclose(f);
    return false;
  }

  fclose(f);

  out_logo->data = (uint8_t *)data;
  out_logo->width = width;
  out_logo->height = height;
  out_logo->valid = true;

  return true;
}

// Save logo to SD card cache
static bool save_sd_cached_logo(const char *team_id, logo_data_t *logo) {
  if (!sd_available || !logo->valid)
    return false;

  char cache_path[64];
  build_cache_path(team_id, cache_path, sizeof(cache_path));

  FILE *f = fopen(cache_path, "wb");
  if (!f) {
    ESP_LOGW(TAG, "Failed to create cache file: %s", cache_path);
    return false;
  }

  // Write header: width (2 bytes) + height (2 bytes)
  uint16_t w = logo->width, h = logo->height;
  fwrite(&w, 2, 1, f);
  fwrite(&h, 2, 1, f);

  // Write RGB565 data
  size_t data_size = logo->width * logo->height * sizeof(uint16_t);
  fwrite(logo->data, 1, data_size, f);

  fclose(f);
  ESP_LOGI(TAG, "Cached logo: %s (%d bytes)", cache_path, (int)(4 + data_size));
  return true;
}

// Flip RGBA image vertically
static void flip_vertical(unsigned char *rgba, int width, int height) {
  int row_size = width * 4;
  unsigned char *temp_row = malloc(row_size);
  if (!temp_row)
    return;

  for (int y = 0; y < height / 2; y++) {
    int top_idx = y * row_size;
    int bottom_idx = (height - 1 - y) * row_size;
    // Swap top and bottom rows
    memcpy(temp_row, &rgba[top_idx], row_size);
    memcpy(&rgba[top_idx], &rgba[bottom_idx], row_size);
    memcpy(&rgba[bottom_idx], temp_row, row_size);
  }
  free(temp_row);
}

// Convert RGBA to grayscale RGB565 with alpha blending to white background
// Special handling: team 2633 renders all opaque pixels as black (silhouette)
static void convert_to_grayscale(unsigned char *rgba, int width, int height,
                                 uint16_t *rgb565_out, const char *team_id) {
  bool render_black = (team_id && strcmp(team_id, "2633") == 0);

  for (int i = 0; i < width * height; i++) {
    int idx = i * 4;
    uint8_t r = rgba[idx];
    uint8_t g = rgba[idx + 1];
    uint8_t b = rgba[idx + 2];
    uint8_t a = rgba[idx + 3]; // Alpha channel

    uint8_t gray;

    if (render_black) {
      // For team 2633: any opaque pixel becomes black, transparent becomes
      // white
      gray = (a > 128) ? 0 : 255;
    } else {
      // Normal processing: blend with white background based on alpha
      if (a < 255) {
        r = (r * a + 255 * (255 - a)) / 255;
        g = (g * a + 255 * (255 - a)) / 255;
        b = (b * a + 255 * (255 - a)) / 255;
      }
      // Convert to grayscale using luminance formula
      gray = (r * 77 + g * 150 + b * 29) >> 8;
    }

    // Convert to RGB565 (grayscale means R=G=B)
    uint16_t rgb565 = ((gray & 0xF8) << 8) | ((gray & 0xFC) << 3) | (gray >> 3);
    rgb565_out[i] = rgb565;
  }
}

bool logo_fetcher_get(const char *url, logo_data_t *out_logo) {
  if (!url || !url[0] || !out_logo) {
    return false;
  }

  memset(out_logo, 0, sizeof(logo_data_t));

  // Extract team ID from URL
  char team_id[16];
  if (!extract_team_id(url, team_id, sizeof(team_id))) {
    ESP_LOGW(TAG, "Could not extract team ID from URL");
    return false;
  }

  // Check in-memory cache first (fastest)
  cached_logo_t *cached = find_cached_logo(team_id);
  if (cached) {
    // Return a copy of cached data
    size_t data_size = cached->width * cached->height * sizeof(uint16_t);
    uint8_t *data_copy = heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM);
    if (data_copy) {
      memcpy(data_copy, cached->data, data_size);
      out_logo->data = data_copy;
      out_logo->width = cached->width;
      out_logo->height = cached->height;
      out_logo->valid = true;
      return true;
    }
  }

  // Try to load from SD card cache
  if (load_sd_cached_logo(team_id, out_logo)) {
    ESP_LOGI(TAG, "Loaded cached logo: %s (%dx%d)", team_id, out_logo->width,
             out_logo->height);
    // Add to in-memory cache for faster subsequent loads
    add_to_cache(team_id, out_logo->data, out_logo->width, out_logo->height);
    return true;
  }

#if !LV_USE_PNG
  ESP_LOGW(TAG, "PNG support not enabled (LV_USE_PNG=n)");
  return false;
#else

  // Build combiner URL for pre-scaled 72x72 image
  char combiner_url[256];
  build_combiner_url(url, combiner_url, sizeof(combiner_url));

  // Download logo from combiner URL (pre-scaled to 72x72)
  char *png_buffer = NULL;
  int png_size = 0;
  int png_cap =
      32 * 1024; // 32KB max for 72x72 logo (much smaller than 500x500)

  ESP_LOGI(TAG, "Downloading logo for team %s...", team_id);

  esp_http_client_config_t config = {
      .url = combiner_url,
      .timeout_ms = 15000,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t err = esp_http_client_open(client, 0);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return false;
  }

  int content_length = esp_http_client_fetch_headers(client);
  if (content_length < 0) {
    content_length = png_cap;
  } else if (content_length > png_cap) {
    ESP_LOGW(TAG, "Logo too large (%d bytes), skipping", content_length);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
  }

  png_buffer = heap_caps_malloc(png_cap, MALLOC_CAP_SPIRAM);
  if (!png_buffer) {
    ESP_LOGE(TAG, "Failed to allocate image buffer");
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
  }

  // Read image data
  while (png_size < png_cap) {
    int read_len =
        esp_http_client_read(client, png_buffer + png_size, png_cap - png_size);
    if (read_len <= 0) {
      break;
    }
    png_size += read_len;
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  if (png_size < 100) { // Minimum valid PNG size
    ESP_LOGE(TAG, "Invalid response - only %d bytes received", png_size);
    free(png_buffer);
    return false;
  }

  ESP_LOGI(TAG, "Downloaded %d bytes", png_size);

  // Decode PNG using LodePNG (will use our custom SPIRAM allocators)
  unsigned char *decoded_data = NULL;
  unsigned width = 0, height = 0;

  unsigned error = lodepng_decode32(&decoded_data, &width, &height,
                                    (unsigned char *)png_buffer, png_size);

  free(png_buffer);

  if (error) {
    ESP_LOGE(TAG, "PNG decode error: %s", lodepng_error_text(error));
    return false;
  }

  ESP_LOGI(TAG, "Decoded PNG: %ux%u", width, height);

  // Calculate scale factor for downscaling to target size
  int scale = 1;
  while ((int)width / scale > TARGET_LOGO_SIZE ||
         (int)height / scale > TARGET_LOGO_SIZE) {
    scale++;
  }

  int out_width = width / scale;
  int out_height = height / scale;

  // Validate size
  if (out_width <= 0 || out_height <= 0 || out_width > MAX_LOGO_WIDTH ||
      out_height > MAX_LOGO_HEIGHT) {
    ESP_LOGW(TAG, "Logo size invalid after scale: %dx%d", out_width,
             out_height);
    free(decoded_data);
    return false;
  }

  // First, downscale the RGBA data
  size_t scaled_size = out_width * out_height * 4;
  unsigned char *scaled_data = heap_caps_malloc(scaled_size, MALLOC_CAP_SPIRAM);
  if (!scaled_data) {
    ESP_LOGE(TAG, "Failed to allocate scaled buffer");
    free(decoded_data);
    return false;
  }

  // Downscale with simple pixel sampling
  for (int y = 0; y < out_height; y++) {
    for (int x = 0; x < out_width; x++) {
      int src_x = x * scale;
      int src_y = y * scale;
      int src_idx = (src_y * width + src_x) * 4;
      int dst_idx = (y * out_width + x) * 4;

      scaled_data[dst_idx + 0] = decoded_data[src_idx + 0]; // R
      scaled_data[dst_idx + 1] = decoded_data[src_idx + 1]; // G
      scaled_data[dst_idx + 2] = decoded_data[src_idx + 2]; // B
      scaled_data[dst_idx + 3] = decoded_data[src_idx + 3]; // A
    }
  }

  free(decoded_data);

  // Allocate RGB565 output buffer
  size_t rgb565_size = out_width * out_height * sizeof(uint16_t);
  uint16_t *rgb565_data = heap_caps_malloc(rgb565_size, MALLOC_CAP_SPIRAM);
  if (!rgb565_data) {
    ESP_LOGE(TAG, "Failed to allocate RGB565 buffer");
    free(scaled_data);
    return false;
  }

  // Special case: flip team 251 vertically
  if (strcmp(team_id, "251") == 0) {
    flip_vertical(scaled_data, out_width, out_height);
  }

  // Convert to grayscale for B/W display (passes team_id for special inversion)
  convert_to_grayscale(scaled_data, out_width, out_height, rgb565_data,
                       team_id);
  free(scaled_data);

  out_logo->data = (uint8_t *)rgb565_data;
  out_logo->width = out_width;
  out_logo->height = out_height;
  out_logo->valid = true;

  ESP_LOGI(TAG, "Converted to %dx%d RGB565 (grayscale)", out_width, out_height);

  // Save to SD card cache for next time
  save_sd_cached_logo(team_id, out_logo);

  // Add to in-memory cache
  add_to_cache(team_id, out_logo->data, out_logo->width, out_logo->height);

  return true;
#endif
}
