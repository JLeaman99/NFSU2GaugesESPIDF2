// image_loader.c
#include "image_loader.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>

#define TAG "IMG_LOADER"

// LVGL v9 bin header layout
typedef struct {
    uint32_t magic;         // 0x19E00000 for LVGL v9
    uint32_t data_size;     // total size of pixel data in bytes
    uint32_t w        : 16;
    uint32_t h        : 16;
    uint32_t cf       : 8;  // colour format
    uint32_t flags    : 24;
    uint32_t stride;        // bytes per row
} __attribute__((packed)) LvglBinHeader;


esp_err_t image_load_from_sd(RamImage *out, const char *sd_path) {
    memset(out, 0, sizeof(RamImage));

    const char *clean = (sd_path[1] == ':') ? sd_path + 2 : sd_path;
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "/sdcard/%s",
             (clean[0] == '/') ? clean + 1 : clean);

    ESP_LOGI(TAG, "Loading: %s", full_path);

    FILE *f = fopen(full_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open: %s", full_path);
        return ESP_ERR_NOT_FOUND;
    }

    // Read full 16 byte header as raw bytes matching exact layout from od output:
    // [0-3]  = magic
    // [4-5]  = width  (little endian uint16)
    // [6-7]  = height (little endian uint16)
    // [8-11] = data_size
    // [12-15]= stride
    uint8_t hdr[16];
    if (fread(hdr, 1, 16, f) != 16) {
        ESP_LOGE(TAG, "Failed to read header");
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    // uint8_t hdr[32];
    // fread(hdr, 1, 32, f);
    // ESP_LOGI(TAG, "Header bytes:");
    // for(int i = 0; i < 32; i += 4) {
    // ESP_LOGI(TAG, "[%02d] %02X %02X %02X %02X", 
    //          i, hdr[i], hdr[i+1], hdr[i+2], hdr[i+3]);
    // }

    // Validate magic first byte
    if (hdr[0] != 0x19) {
        ESP_LOGE(TAG, "Invalid magic byte: 0x%02X", hdr[0]);
        fclose(f);
        return ESP_ERR_INVALID_VERSION;
    }

    // Extract fields from exact byte positions
   uint8_t  cf     = hdr[1];
uint16_t w      = hdr[4] | (hdr[5] << 8);
uint16_t h      = hdr[6] | (hdr[7] << 8);
uint32_t stride = hdr[8] | (hdr[9] << 8) | (hdr[10] << 16) | (hdr[11] << 24);

// Calculate data_size from stride and height — pixel data starts at byte 12
uint32_t data_size = stride * h;

ESP_LOGI(TAG, "cf=%d w=%d h=%d stride=%lu data_size=%lu", 
         cf, w, h, stride, data_size);

// Sanity check
if (w == 0 || h == 0 || w > 4096 || h > 4096 || data_size > 20000000) {
    ESP_LOGE(TAG, "Implausible values");
    fclose(f);
    return ESP_ERR_INVALID_ARG;
}

// Allocate in PSRAM
out->buf_size = data_size;
out->buf = heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM);
if (!out->buf) {
    ESP_LOGE(TAG, "PSRAM alloc failed for %lu bytes. Free PSRAM: %zu",
             data_size, heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    out->buf = malloc(data_size);
    if (!out->buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
}

// Pixel data starts at byte 12 — already at correct position after fread of 16 bytes
// Seek back to byte 12 to read pixel data
fseek(f, 12, SEEK_SET);
size_t bytes_read = fread(out->buf, 1, data_size, f);
fclose(f);

if (bytes_read != data_size) {
    ESP_LOGE(TAG, "Read %zu of %lu bytes", bytes_read, data_size);
    free(out->buf);
    out->buf = NULL;
    return ESP_ERR_INVALID_SIZE;
}

// Populate LVGL descriptor
out->dsc.header.cf     = (lv_color_format_t)cf;
out->dsc.header.w      = w;
out->dsc.header.h      = h;
out->dsc.header.stride = stride;
out->dsc.data_size     = data_size;
out->dsc.data          = out->buf;
out->loaded            = true;

ESP_LOGI(TAG, "Loaded %dx%d cf=%d stride=%lu (%lu bytes in PSRAM)",  w, h, cf, stride, data_size);
    return ESP_OK;
}

void image_unload(RamImage *img) {
    if (img->buf) {
        free(img->buf);
        img->buf = NULL;
    }
    img->loaded = false;
}