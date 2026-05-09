// image_loader.h
#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include "lvgl.h"

// Holds a RAM-resident copy of an LVGL bin image
typedef struct {
    lv_image_dsc_t  dsc;        // LVGL image descriptor pointing at buf
    uint8_t        *buf;        // PSRAM buffer holding raw pixel data
    size_t          buf_size;
    bool            loaded;
} RamImage;

esp_err_t image_load_from_sd(RamImage *out, const char *sd_path);
void      image_unload(RamImage *img);

#endif