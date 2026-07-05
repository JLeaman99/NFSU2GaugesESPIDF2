#pragma once

#include "lvgl.h"
#include "image_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Predefined colour presets ──────────────────────────────────────────────
typedef struct {
    const char  *name;          // display name, e.g. "Amber"
    lv_color_t   face_tint;     // recolour applied to face layer
    lv_opa_t     face_tint_opa;
    lv_color_t   mid_tint;      // recolour applied to mid layer
    lv_opa_t     mid_tint_opa;
} ThemeColor;

// ── Gauge-face descriptors ─────────────────────────────────────────────────
typedef struct {
    const char *name;           // display name, e.g. "Tabby"
    const char *face_bin;       // SD path  "S:themes/tabby/face.bin"
    const char *mid_bin;
    const char *needle_bin;
} GaugeFace;

// ── Menu callbacks (implement in main.c) ───────────────────────────────────
/**
 * Called when the user confirms a new face + colour selection.
 * The menu is already dismissed before this fires; redraw the dash here.
 */
typedef void (*menu_apply_cb_t)(const GaugeFace *face, const ThemeColor *color);

// ── Public API ─────────────────────────────────────────────────────────────

/**
 * One-time init.  Call after bsp_display_start_with_config().
 * @param apply_cb  Your function that reloads images and calls FlattenStaticLayers().
 */
void menu_init(menu_apply_cb_t apply_cb);

/**
 * Show the overlay from anywhere (e.g. a long-press handler on the dash screen).
 */
void menu_show(void);

/**
 * Hide/destroy the overlay without applying changes.
 */
void menu_hide(void);

#ifdef __cplusplus
}
#endif
