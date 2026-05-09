// ============================================================
// menu.c
// ============================================================
#include "menu.h"
#include "lvgl.h"

// ---- Internal state ----------------------------------------
typedef enum { ELEMENT_NEEDLE, ELEMENT_FACE } SelectedElement;

static struct {
    lv_obj_t *needle_img;
    lv_obj_t *face_img;
    lv_anim_t *needle_anim;

    // Menu panels (created/deleted on open/close)
    lv_obj_t *overlay;
    lv_obj_t *panel;

    // Pending selections (applied only on confirm)
    SelectedElement  active_element;
    const lv_image_dsc_t *pending_img_src;
    lv_color_t       pending_colour;
    bool             colour_selected;
    lv_anim_exec_xcb_t angle_cb;   // ← add this

} menu;

// ---- Forward declarations ----------------------------------
static void open_menu(lv_event_t *e);
static void close_menu(void);
static void build_element_page(void);
static void build_image_page(void);
static void build_colour_page(void);
static void on_element_selected(lv_event_t *e);
static void on_image_selected(lv_event_t *e);
static void on_colour_selected(lv_event_t *e);
static void on_confirm(lv_event_t *e);
static void on_back(lv_event_t *e);
static void apply_selections(void);
static lv_obj_t *create_panel(void);
static lv_obj_t *add_back_button(lv_obj_t *parent);

// ---- Helpers -----------------------------------------------

// Pause animation by setting speed to 0
static void pause_animation(void) {
    lv_anim_del(menu.needle_img, NULL);
}

// Restart the needle animation
static void resume_animation(void) {
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, menu.needle_img);
    lv_anim_set_exec_cb(&anim, menu.angle_cb);
    lv_anim_set_values(&anim, 0, 2400);
    lv_anim_set_duration(&anim, 5000);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);
}

// Semi-transparent dark overlay behind the panel
static lv_obj_t *create_overlay(void) {
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    return overlay;
}

// Centred floating panel
static lv_obj_t *create_panel(void) {
    lv_obj_t *panel = lv_obj_create(menu.overlay);
    lv_obj_set_size(panel, 400, 450);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_make(30, 30, 30), 0);
    lv_obj_set_style_border_color(panel, lv_color_make(80, 80, 80), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(panel, 16, 0);
    lv_obj_set_style_pad_gap(panel, 12, 0);
    return panel;
}

// Back button row
static lv_obj_t *add_back_button(lv_obj_t *parent) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_style_bg_color(btn, lv_color_make(60, 60, 60), 0);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, on_back, LV_EVENT_CLICKED, NULL);
    return btn;
}

// ---- Page builders -----------------------------------------

// Page 1: Choose which element to customise
static void build_element_page(void) {
    lv_obj_clean(menu.panel);

    lv_obj_t *title = lv_label_create(menu.panel);
    lv_label_set_text(title, "Customise");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    const char *labels[] = { "Needle", "Gauge Face" };
    for (int i = 0; i < 2; i++) {
        lv_obj_t *btn = lv_button_create(menu.panel);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_style_bg_color(btn, lv_color_make(50, 50, 80), 0);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, on_element_selected, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    // Close button
    lv_obj_t *close_btn = lv_button_create(menu.panel);
    lv_obj_set_width(close_btn, LV_PCT(100));
    lv_obj_set_style_bg_color(close_btn, lv_color_make(80, 30, 30), 0);
    lv_obj_t *close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "Close");
    lv_obj_center(close_lbl);
    lv_obj_add_event_cb(close_btn, on_back, LV_EVENT_CLICKED, (void *)1); // flag=1 means full close
}

// Page 2: Choose image variant
static void build_image_page(void) {
    lv_obj_clean(menu.panel);

    lv_obj_t *title = lv_label_create(menu.panel);
    lv_label_set_text(title, menu.active_element == ELEMENT_NEEDLE ? "Select Needle" : "Select Face");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    const ImageOption *options = (menu.active_element == ELEMENT_NEEDLE) ? needle_options : face_options;
    int count                  = (menu.active_element == ELEMENT_NEEDLE) ? NEEDLE_OPTION_COUNT : FACE_OPTION_COUNT;

    for (int i = 0; i < count; i++) {
        lv_obj_t *btn = lv_button_create(menu.panel);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_style_bg_color(btn, lv_color_make(50, 50, 80), 0);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, options[i].label);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, on_image_selected, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    add_back_button(menu.panel);
}

// Page 3: Choose colour
static void build_colour_page(void) {
    lv_obj_clean(menu.panel);

    lv_obj_t *title = lv_label_create(menu.panel);
    lv_label_set_text(title, "Select Colour");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    // Colour swatches in a grid row
    lv_obj_t *grid = lv_obj_create(menu.panel);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_height(grid, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_gap(grid, 10, 0);

    for (int i = 0; i < PALETTE_SIZE; i++) {
        lv_obj_t *swatch = lv_button_create(grid);
        lv_obj_set_size(swatch, 50, 50);
        lv_obj_set_style_bg_color(swatch, colour_palette[i], 0);
        lv_obj_set_style_radius(swatch, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(swatch, lv_color_white(), 0);
        lv_obj_set_style_border_width(swatch, 2, 0);
        lv_obj_add_event_cb(swatch, on_colour_selected, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    // Confirm button (disabled until colour is picked)
    lv_obj_t *confirm_btn = lv_button_create(menu.panel);
    lv_obj_set_width(confirm_btn, LV_PCT(100));
    lv_obj_set_style_bg_color(confirm_btn, lv_color_make(30, 100, 30), 0);
    lv_obj_add_flag(confirm_btn, LV_OBJ_FLAG_HIDDEN); // shown after colour tap
    lv_obj_t *confirm_lbl = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_lbl, LV_SYMBOL_OK " Apply");
    lv_obj_center(confirm_lbl);
    lv_obj_add_event_cb(confirm_btn, on_confirm, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(confirm_btn, confirm_btn); // store ref for colour cb

    // Store confirm btn ref on grid so colour cb can find it
    lv_obj_set_user_data(grid, confirm_btn);

    add_back_button(menu.panel);
}

// ---- Event callbacks ---------------------------------------

static void open_menu(lv_event_t *e) {
    if (menu.overlay != NULL) return; // already open
    pause_animation();
    menu.overlay = create_overlay();
    menu.panel   = create_panel();
    build_element_page();
}

static void close_menu(void) {
    if (menu.overlay) {
        lv_obj_delete(menu.overlay);
        menu.overlay = NULL;
        menu.panel   = NULL;
    }
    resume_animation();
}

static void on_back(lv_event_t *e) {
    // flag=1 means close entirely (used by Close button on page 1)
    intptr_t flag = (intptr_t)lv_event_get_user_data(e);
    if (flag == 1) {
        close_menu();
    } else {
        build_element_page();
    }
}

static void on_element_selected(lv_event_t *e) {
    menu.active_element = (SelectedElement)(intptr_t)lv_event_get_user_data(e);
    build_image_page();
}

static void on_image_selected(lv_event_t *e) {
    int idx = (intptr_t)lv_event_get_user_data(e);
    const ImageOption *options = (menu.active_element == ELEMENT_NEEDLE) ? needle_options : face_options;
    menu.pending_img_src = options[idx].src;
    menu.colour_selected = false;
    build_colour_page();
}

static void on_colour_selected(lv_event_t *e) {
    int idx = (intptr_t)lv_event_get_user_data(e);
    menu.pending_colour  = colour_palette[idx];
    menu.colour_selected = true;

    // Highlight selected swatch and reveal confirm button
    lv_obj_t *swatch = lv_event_get_target(e);
    lv_obj_t *grid   = lv_obj_get_parent(swatch);

    // Reset all swatch borders
    uint32_t child_count = lv_obj_get_child_count(grid);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_set_style_border_width(lv_obj_get_child(grid, i), 2, 0);
        lv_obj_set_style_border_color(lv_obj_get_child(grid, i), lv_color_white(), 0);
    }
    // Highlight tapped swatch
    lv_obj_set_style_border_width(swatch, 4, 0);
    lv_obj_set_style_border_color(swatch, lv_color_white(), 0);

    // Show confirm button
    lv_obj_t *confirm_btn = (lv_obj_t *)lv_obj_get_user_data(grid);
    lv_obj_clear_flag(confirm_btn, LV_OBJ_FLAG_HIDDEN);
}

static void on_confirm(lv_event_t *e) {
    apply_selections();
    close_menu();
}

// ---- Apply selections to live objects ----------------------
static void apply_selections(void) {
    lv_obj_t *target = (menu.active_element == ELEMENT_NEEDLE) ? menu.needle_img : menu.face_img;

    // Swap image source
    lv_image_set_src(target, menu.pending_img_src);

    // Apply recolour
    lv_obj_set_style_image_recolor(target, menu.pending_colour, 0);
    lv_obj_set_style_image_recolor_opa(target, LV_OPA_50, 0); // blend strength, adjust to taste
}

// ---- Public init -------------------------------------------
void menu_init(lv_obj_t *needle_img, lv_obj_t *face_img, lv_anim_exec_xcb_t angle_cb) {
    menu.needle_img  = needle_img;
    menu.face_img    = face_img;
    //menu.needle_anim = needle_anim;
    menu.overlay     = NULL;
    menu.panel       = NULL;
    menu.angle_cb   = angle_cb;    // ← store it

    // Attach tap handler to the screen
    lv_obj_add_event_cb(lv_scr_act(), open_menu, LV_EVENT_CLICKED, NULL);
}