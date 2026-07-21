/**
 * menu.c – Two-step overlay menu for the ESP32-P4 instrument cluster.
 *
 * Step 1: Pick a gauge face  (horizontal swipe carousel)
 * Step 2: Pick a colour      (grid of colour swatches)
 *
 * A single "Accept" button commits the selection and fires menu_apply_cb_t.
 * A "Back" button goes from step 2 → step 1.
 * Tapping outside the panel (or the X) dismisses without changes.
 *
 * All LVGL calls must be made while the display lock is held; the caller is
 * responsible for that (DrawDash already wraps in bsp_display_lock).
 */

#include "menu.h"
#include "esp_log.h"
#include <string.h>

#define TAG "MENU"

// ── Catalogue ─────────────────────────────────────────────────────────────

static const GaugeFace FACES[] = {
    { "Tabby",   "S:themes/tabby/face.bin",   "S:themes/tabby/mid.bin",   "S:themes/tabby/needle.bin"   },
    { "Flames", "S:themes/flames/face.bin", "S:themes/flames/mid.bin", "S:themes/flames/needle.bin" },
    { "Minimal", "S:themes/minimal/face.bin", "S:themes/minimal/mid.bin", "S:themes/minimal/needle.bin" },
};
#define FACE_COUNT (sizeof(FACES) / sizeof(FACES[0]))


static ThemeColor COLORS[] = {
    { "Default",   {}, LV_OPA_50, {}, LV_OPA_50 },
    { "White",   {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Red",   {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Rose",   {}, LV_OPA_50, {}, LV_OPA_50 },   // filled below in menu_init
    { "Orange",     {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Yellow",    {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Lime",    {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Green",  {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Navy",   {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Ice",   {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Cyan",   {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Blue",   {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Purple",   {}, LV_OPA_50, {}, LV_OPA_50 },
    { "Magenta",   {}, LV_OPA_50, {}, LV_OPA_50 },
   
};
#define COLOR_COUNT (sizeof(COLORS) / sizeof(COLORS[0]))

// Raw hex values kept separately so the struct can have lv_color_t later
static const uint32_t COLOR_FACE_HEX[]  = { 0x001122, 0xFFFFFF, 0xFF0000, 0xFF0064, 0xFF4200, 0xFFC900, 0x66FF44, 0x1BF734, 0x000396,0x4488FF, 0x00FFFF, 0x0000FF, 0x8B00FF, 0xFF00FF};

//static const uint32_t COLOR_FACE_HEX[]  = { 0x001122, 0xC8C566, 0x66C8C5, 0xFF0064, 0x66FF44, 0x4488FF, 0xCECECE};
//static const uint32_t COLOR_MID_HEX[]   = { 0x001122, 0xFF0064, 0x0064FF, 0xFF6600, 0xFF0064, 0xFF0099, 0xCECECE};

static const uint32_t COLOR_MID_HEX[]   = { 0x001122, 0xFFFFFF, 0xFF0000, 0xFF0064, 0xFF4200, 0xFFC900, 0x66FF44, 0x1BF734, 0x000396,0x4488FF, 0x00FFFF, 0x0000FF, 0x8B00FF, 0xFF00FF};

// ── Internal state ─────────────────────────────────────────────────────────

static menu_apply_cb_t s_apply_cb  = NULL;
static lv_obj_t       *s_overlay   = NULL;   // full-screen dim container
static lv_obj_t       *s_panel     = NULL;   // the white card

static int s_sel_face  = 0;   // currently highlighted face index
static int s_sel_face_color = 0;   // currently highlighted color index
static int s_sel_mid_color = 0;   // currently highlighted color index
static int s_step      = 1;   // 1 = face picker, 2 = color picker

// Per-step content containers (swapped in/out)
static lv_obj_t *s_step1_cont = NULL;
static lv_obj_t *s_step2_cont = NULL;
static lv_obj_t *s_step3_cont = NULL;

static lv_obj_t *s_accept_btn = NULL;
static lv_obj_t *s_back_btn   = NULL;

// Keep references to face + color buttons so we can highlight the selection
static lv_obj_t *s_face_btns [FACE_COUNT];
static lv_obj_t *s_color_btns[COLOR_COUNT];
static lv_obj_t *s_mid_color_btns[COLOR_COUNT];  // step 3 mid colour buttons

static lv_obj_t *s_step1_label = NULL;
static lv_obj_t *s_step2_label = NULL;
static lv_obj_t *s_step3_label = NULL;
// ── Helpers ────────────────────────────────────────────────────────────────

#define PANEL_W   680
#define PANEL_H   520
#define RADIUS    18

#define COL_BG        lv_color_make(0x18, 0x18, 0x18)  // near-black panel bg
#define COL_CARD      lv_color_make(0x26, 0x26, 0x26)  // face card bg
#define COL_ACCENT    lv_color_make(0xC8, 0xC5, 0x66)  // amber selection ring
#define COL_BTN       lv_color_make(0x38, 0x38, 0x38)
#define COL_BTN_ACT   lv_color_make(0xC8, 0xC5, 0x66)
#define COL_TEXT      lv_color_make(0xFF, 0xFF, 0xFF)
#define COL_SUBTEXT   lv_color_make(0x88, 0x88, 0x88)
#define COL_DIM       lv_color_make(0x00, 0x00, 0x00)

static void style_btn(lv_obj_t *btn, bool active)
{
    lv_obj_set_style_bg_color(btn, active ? COL_BTN_ACT : COL_BTN, 0);
    lv_obj_set_style_border_color(btn, active ? COL_ACCENT : lv_color_make(0x44,0x44,0x44), 0);
    lv_obj_set_style_border_width(btn, active ? 3 : 1, 0);
    lv_obj_set_style_radius(btn, 10, 0);
}

// ── Face card ─────────────────────────────────────────────────────────────

static void face_btn_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    s_sel_face = idx;

    // Refresh all face-button highlight states
    for (int i = 0; i < (int)FACE_COUNT; i++) {
        style_btn(s_face_btns[i], i == s_sel_face);
    }
}

static lv_obj_t *build_step1(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, PANEL_W - 40, 340);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_NONE);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 60);

    // Horizontal flex row of face cards
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont, 16, 0);

    int card_w = (PANEL_W - 40 - (int)(FACE_COUNT - 1) * 16) / (int)FACE_COUNT;
    card_w = card_w > 180 ? 180 : card_w;

    for (int i = 0; i < (int)FACE_COUNT; i++) {
        lv_obj_t *card = lv_obj_create(cont);
        lv_obj_set_size(card, card_w, 200);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_set_scroll_dir(card, LV_DIR_NONE);
        style_btn(card, i == s_sel_face);
        lv_obj_set_style_bg_color(card, COL_CARD, 0);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, face_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        // Gauge name label
        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, FACES[i].name);
        lv_obj_set_style_text_color(name, COL_TEXT, 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
        lv_obj_align(name, LV_ALIGN_CENTER, 0, 0);

        s_face_btns[i] = card;
    }

    // Step label
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "SELECT GAUGE");
    lv_obj_set_style_text_color(lbl, COL_SUBTEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 20);
    s_step1_label = lbl;
    return cont;
}

// ── Colour swatch ─────────────────────────────────────────────────────────

static void mid_color_btn_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    s_sel_mid_color = idx;

    for (int i = 0; i < (int)COLOR_COUNT; i++) {
        // Keep the swatch's actual colour; just vary the border
        lv_obj_set_style_border_color(s_mid_color_btns[i],
            i == s_sel_mid_color ? COL_ACCENT : lv_color_make(0x44,0x44,0x44), 0);
        lv_obj_set_style_border_width(s_mid_color_btns[i], i == s_sel_mid_color ? 4 : 1, 0);
    }
}

static void face_color_btn_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    s_sel_face_color = idx;

    for (int i = 0; i < (int)COLOR_COUNT; i++) {
        // Keep the swatch's actual colour; just vary the border
        lv_obj_set_style_border_color(s_color_btns[i],
            i == s_sel_face_color ? COL_ACCENT : lv_color_make(0x44,0x44,0x44), 0);
        lv_obj_set_style_border_width(s_color_btns[i], i == s_sel_face_color ? 4 : 1, 0);
    }
}

static lv_obj_t *build_step2(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, PANEL_W - 40, 340);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);

    lv_obj_set_scroll_dir(cont, LV_DIR_VER);          // vertical scroll only
    lv_obj_set_style_max_height(cont, 380, 0);         // cap height to panel space
    lv_obj_set_scroll_snap_y(cont, LV_SCROLL_SNAP_NONE);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 60);

    // 3-column flex grid
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont, 14, 0);

    for (int i = 0; i < (int)COLOR_COUNT; i++) {
        lv_obj_t *swatch = lv_obj_create(cont);
        lv_obj_set_size(swatch, 120, 120);
        lv_obj_set_scroll_dir(swatch, LV_DIR_NONE);
        lv_obj_set_style_radius(swatch, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(swatch, lv_color_hex(COLOR_FACE_HEX[i]), 0);
        lv_obj_set_style_border_color(swatch,
            i == s_sel_face_color ? COL_ACCENT : lv_color_make(0x44,0x44,0x44), 0);
        lv_obj_set_style_border_width(swatch, i == s_sel_face_color ? 4 : 1, 0);
        lv_obj_add_flag(swatch, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(swatch, face_color_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        // Colour name label
        lv_obj_t *name = lv_label_create(swatch);
        lv_label_set_text(name, COLORS[i].name);
        lv_obj_set_style_text_color(name, COL_TEXT, 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_obj_align(name, LV_ALIGN_CENTER, 0, -8);

        s_color_btns[i] = swatch;
    }

    // Step label
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "SELECT FACE COLOUR");
    lv_obj_set_style_text_color(lbl, COL_SUBTEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 20);
    s_step2_label = lbl;
    return cont;
}

static lv_obj_t *build_step3(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, PANEL_W - 40, 340);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);          // vertical scroll only
    lv_obj_set_style_max_height(cont, 380, 0);         // cap height to panel space
    lv_obj_set_scroll_snap_y(cont, LV_SCROLL_SNAP_NONE);
    

    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 60);

    // 3-column flex grid
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cont, 14, 0);

    for (int i = 0; i < (int)COLOR_COUNT; i++) {
        lv_obj_t *swatch = lv_obj_create(cont);
        lv_obj_set_size(swatch, 120, 120);
        lv_obj_set_scroll_dir(swatch, LV_DIR_NONE);
        lv_obj_set_style_radius(swatch, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(swatch, lv_color_hex(COLOR_MID_HEX[i]), 0);
        lv_obj_set_style_border_color(swatch,
            i == s_sel_mid_color ? COL_ACCENT : lv_color_make(0x44,0x44,0x44), 0);
        lv_obj_set_style_border_width(swatch, i == s_sel_mid_color ? 4 : 1, 0);
        lv_obj_add_flag(swatch, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(swatch, mid_color_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        // Colour name label
        lv_obj_t *name = lv_label_create(swatch);
        lv_label_set_text(name, COLORS[i].name);
        lv_obj_set_style_text_color(name, COL_TEXT, 0);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
        lv_obj_align(name, LV_ALIGN_CENTER, 0, -8);

        s_mid_color_btns[i] = swatch;
    }

    // Step label
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "SELECT MID COLOUR");
    lv_obj_set_style_text_color(lbl, COL_SUBTEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 20);
    s_step3_label = lbl;
    return cont;
}

// ── Navigation ─────────────────────────────────────────────────────────────

static void go_step3(void)
{
    s_step = 3;
    lv_obj_add_flag(s_step1_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_step2_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_back_btn,  LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(s_accept_btn, 0), "APPLY");
        // Hide step 1 content and label
    lv_obj_add_flag(s_step1_cont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_step1_label, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(s_step2_cont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_step2_label, LV_OBJ_FLAG_HIDDEN);
    // Show step 2 content and label
    lv_obj_clear_flag(s_step3_cont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_step3_label, LV_OBJ_FLAG_HIDDEN);
}

static void go_step2(void)
{
    s_step = 2;
    lv_obj_add_flag(s_step1_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_step2_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_back_btn,  LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(s_accept_btn, 0), "NEXT");
        // Hide step 1 content and label
    lv_obj_add_flag(s_step1_cont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_step1_label, LV_OBJ_FLAG_HIDDEN);
    

    // Show step 2 content and label
    lv_obj_clear_flag(s_step2_cont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_step2_label, LV_OBJ_FLAG_HIDDEN);
}

static void go_step1(void)
{
    s_step = 1;
    lv_obj_clear_flag(s_step1_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_step2_cont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_back_btn,    LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(s_accept_btn, 0), "NEXT");

        // Show step 1 content and label
    lv_obj_clear_flag(s_step1_cont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_step1_label, LV_OBJ_FLAG_HIDDEN);

    // Hide step 2 content and label
    lv_obj_add_flag(s_step2_cont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_step2_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_step3_cont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_step3_label, LV_OBJ_FLAG_HIDDEN);
}

// ── Button callbacks ───────────────────────────────────────────────────────

static void accept_cb(lv_event_t *e)
{
    if (s_step == 1) {
        go_step2();
        return;
    }

    if (s_step == 2) {
        go_step3();
        return;
    }

    // Step 3: commit

        static ThemeColor result;
    result.name      = COLORS[s_sel_face_color].name;
    result.face_tint = lv_color_hex(COLOR_FACE_HEX[s_sel_face_color]);
    result.face_tint_opa  = LV_OPA_50;
    result.mid_tint  = lv_color_hex(COLOR_MID_HEX[s_sel_mid_color]);
    result.mid_tint_opa   = LV_OPA_50;
    result.retint    = (COLOR_FACE_HEX[s_sel_face_color] != 0x001122 ||
                        COLOR_MID_HEX[s_sel_mid_color]   != 0x001122);

    menu_hide();

    if (s_apply_cb) {
        s_apply_cb(&FACES[s_sel_face], &result);
    }
}

static void back_cb(lv_event_t *e)
{
    go_step1();
}

static void dismiss_cb(lv_event_t *e)
{
    // Only dismiss when clicking the dim overlay itself, not bubbled events
    if (lv_event_get_target(e) == s_overlay) {
        menu_hide();
    }
}

static void close_btn_cb(lv_event_t *e)
{
    menu_hide();
}

// ── Public API ─────────────────────────────────────────────────────────────

void menu_init(menu_apply_cb_t apply_cb)
{
    s_apply_cb = apply_cb;

    // Populate lv_color_t fields from the hex arrays
    for (int i = 0; i < (int)COLOR_COUNT; i++) {
        // Cast away const – we own the array, it is static storage not ROM
        ThemeColor *c = (ThemeColor *)&COLORS[i];
        c->face_tint = lv_color_hex(COLOR_FACE_HEX[i]);
        c->mid_tint  = lv_color_hex(COLOR_MID_HEX[i]);
        if(COLOR_FACE_HEX[i] == 0x001122 && COLOR_MID_HEX[i] == 0x001122) {
            c->retint = false; // Mono theme, don't recolour static layers
        } else {
            c->retint = true;  // Recolour static layers
        }
    }

    ESP_LOGI(TAG, "menu_init: %d faces, %d colours", (int)FACE_COUNT, (int)COLOR_COUNT);
}

static void panel_click_absorb_cb(lv_event_t *e)
{
    // Absorb the click so it does not bubble up to the dim overlay dismiss handler
    (void)e;
}

void menu_show(void)
{
    if (s_overlay != NULL) return;   // already open

    // ── Dim overlay (full-screen, semi-transparent) ──────────────────────
    s_overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_overlay, COL_DIM, 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_radius(s_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_overlay, 0, 0);
    lv_obj_set_scroll_dir(s_overlay, LV_DIR_NONE);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_overlay, dismiss_cb, LV_EVENT_CLICKED, NULL);

    // ── Panel card ───────────────────────────────────────────────────────
    s_panel = lv_obj_create(s_overlay);
    lv_obj_set_size(s_panel, PANEL_W, PANEL_H);
    lv_obj_center(s_panel);
    lv_obj_set_style_bg_color(s_panel, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_panel, RADIUS, 0);
    lv_obj_set_style_border_color(s_panel, lv_color_make(0x44,0x44,0x44), 0);
    lv_obj_set_style_border_width(s_panel, 1, 0);
    lv_obj_set_style_pad_all(s_panel, 20, 0);
    lv_obj_set_scroll_dir(s_panel, LV_DIR_NONE);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Stop click events bubbling up to the dim-overlay dismiss handler
    lv_obj_add_event_cb(s_panel, panel_click_absorb_cb, LV_EVENT_CLICKED, NULL);

    // ── Close (×) button ─────────────────────────────────────────────────
    lv_obj_t *close_btn = lv_btn_create(s_panel);
    lv_obj_set_size(close_btn, 40, 40);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(close_btn, COL_BTN, 0);
    lv_obj_set_style_radius(close_btn, 8, 0);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, close_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_lbl, COL_TEXT, 0);
    lv_obj_center(close_lbl);

    // ── Step content ─────────────────────────────────────────────────────
    s_step1_cont = build_step1(s_panel);
    s_step2_cont = build_step2(s_panel);
    s_step3_cont = build_step3(s_panel);
    lv_obj_add_flag(s_step2_cont, LV_OBJ_FLAG_HIDDEN);  // hidden initially
    lv_obj_add_flag(s_step2_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_step3_cont, LV_OBJ_FLAG_HIDDEN);  // hidden initially
    lv_obj_add_flag(s_step3_label, LV_OBJ_FLAG_HIDDEN);


    // ── Bottom button row ─────────────────────────────────────────────────
    // Back button (hidden on step 1)
    s_back_btn = lv_btn_create(s_panel);
    lv_obj_set_size(s_back_btn, 140, 52);
    lv_obj_align(s_back_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_back_btn, COL_BTN, 0);
    lv_obj_set_style_radius(s_back_btn, 10, 0);
    lv_obj_set_style_border_width(s_back_btn, 0, 0);
    lv_obj_add_flag(s_back_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_back_btn, back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(s_back_btn);
    lv_label_set_text(back_lbl, "BACK");
    lv_obj_set_style_text_color(back_lbl, COL_TEXT, 0);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(back_lbl);

    // Accept / Next button
    s_accept_btn = lv_btn_create(s_panel);
    lv_obj_set_size(s_accept_btn, 160, 52);
    lv_obj_align(s_accept_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(s_accept_btn, COL_ACCENT, 0);
    lv_obj_set_style_radius(s_accept_btn, 10, 0);
    lv_obj_set_style_border_width(s_accept_btn, 0, 0);
    lv_obj_add_event_cb(s_accept_btn, accept_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *accept_lbl = lv_label_create(s_accept_btn);
    lv_label_set_text(accept_lbl, "NEXT");
    lv_obj_set_style_text_color(accept_lbl, lv_color_make(0x18,0x18,0x18), 0);
    lv_obj_set_style_text_font(accept_lbl, &lv_font_montserrat_18, 0);
    lv_obj_center(accept_lbl);

    s_step = 1;
    ESP_LOGI(TAG, "Menu shown");
}

void menu_hide(void)
{
    if (s_overlay == NULL) return;

    lv_obj_delete(s_overlay);
    s_overlay    = NULL;
    s_panel      = NULL;
    s_step1_cont = NULL;
    s_step2_cont = NULL;
    s_accept_btn = NULL;
    s_back_btn   = NULL;
    memset(s_face_btns,  0, sizeof(s_face_btns));
    memset(s_color_btns, 0, sizeof(s_color_btns));
    memset(s_mid_color_btns, 0, sizeof(s_mid_color_btns));
    s_sel_face_color = 0;
    s_sel_mid_color  = 0;

    ESP_LOGI(TAG, "Menu hidden");
}
