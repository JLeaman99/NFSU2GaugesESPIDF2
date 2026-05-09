// ============================================================
// menu.h
// ============================================================
#ifndef MENU_H
#define MENU_H

#include "lvgl.h"

// Colour palette options
#define PALETTE_SIZE 6
static const lv_color_t colour_palette[PALETTE_SIZE] = {
    LV_COLOR_MAKE(255, 255, 255),  // White
    LV_COLOR_MAKE(255, 50,  50),   // Red
    LV_COLOR_MAKE(50,  150, 255),  // Blue
    LV_COLOR_MAKE(50,  255, 100),  // Green
    LV_COLOR_MAKE(255, 200, 0),    // Amber
    LV_COLOR_MAKE(200, 50,  255),  // Purple
};

// Image variants per element — point these at your actual image sources
extern const lv_image_dsc_t needle_v1, needle_v2, needle_v3;
extern const lv_image_dsc_t face_v1,   face_v2,   face_v3;

typedef struct {
    const lv_image_dsc_t *src;
    const char           *label;
} ImageOption;

static const ImageOption needle_options[] = {
    { &needle_v1, "Needle 1" },
    { &needle_v2, "Needle 2" },
    { &needle_v3, "Needle 3" },
};

static const ImageOption face_options[] = {
    { &face_v1, "Face 1" },
    { &face_v2, "Face 2" },
    { &face_v3, "Face 3" },
};

#define NEEDLE_OPTION_COUNT 3
#define FACE_OPTION_COUNT   3

void menu_init(lv_obj_t *needle_img, lv_obj_t *face_img, lv_anim_exec_xcb_t angle_cb);

#endif