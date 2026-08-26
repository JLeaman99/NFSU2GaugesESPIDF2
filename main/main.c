#pragma region Imports
#define BSP_LCD_DRAW_BUFF_SIZE  (800 * 150)   // 150 lines at a time

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "lv_demos.h"
#include "menu.h"
#include "image_loader.h"

//SD Card Imports
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

//#include "sd_test_io.h"
//#include "SD.h"

//CAN imports
#include "driver/twai.h"
//#include "esp_twai.h"
#include "driver/gpio.h"
#include "helvetica_60.h"
#include "helvetica_B_I_70.h"
#include "helvetica_B_I_90.h"

//#include "needle.h"
#include "FUELfull.h"
#include "face.h"
#include "mid.h"
//#include "HUD.h"
#pragma endregion



#define DEBUG_DASH 1

//CAN VARIABLES
#define CAN_TX_GPIO (gpio_num_t)5
#define CAN_RX_GPIO (gpio_num_t)4
#define RX_TASK_PRIO            8
 int countTry = 0;

//FS Driver
#define MOUNT_POINT "/sdcard"
static const char *TAG = "example";

#pragma region Image Declare

RamImage ram_background, ram_middle, ram_needle, ram_hud, ram_fuel;
lv_obj_t *static_canvas = NULL;
lv_obj_t *fuel_arc = NULL;

// LV_IMG_DECLARE(FUELfull);
// LV_IMG_DECLARE(face);
// LV_IMG_DECLARE(mid);
// LV_IMG_DECLARE(needle);
//LV_IMG_DECLARE(HUD);

// LV_FONT_DECLARE(helvetica_60);
// LV_FONT_DECLARE(helvetica_B_I_70);
#pragma endregion

TaskHandle_t TWAI_Recieve_Task;

lv_obj_t *face_img, *mid_img, *needle_img, *fuel_img, *hud_img;
static lv_obj_t *speed_label, *unit_label, *gear_label;

// Define colors manually
#define LV_COLOR_RED lv_color_make(0xFF, 0x00, 0x00)
#define LV_BACK_COLOR lv_color_make(0x1b,0x1b,0x1b)
#define LV_COLOR_GREEN lv_color_make(0x00, 0xFF, 0x00)
#define LV_COLOR_BLUE lv_color_make(0x00, 0x00, 0xFF)
#define LV_COLOR_WHITE lv_color_make(0xFF, 0xFF, 0xFF)
#define LV_COLOR_BLACK lv_color_make(0x00, 0x00, 0x00) // Define black color
#define DEFAULTCOLOUR lv_color_hex(0x001122)


typedef enum {
    GEAR_REVERSE = -1,
    GEAR_NEUTRAL =  0,
    GEAR_1       =  1,
    GEAR_2       =  2,
    GEAR_3       =  3,
    GEAR_4       =  4,
    GEAR_5       =  5,
    GEAR_6       =  6,
} Gear;


// Function to fill the screen with a specific color and clear previous content
void fill_screen_with_color(lv_color_t color)
{
    lv_obj_t *scr = lv_scr_act();                             // Get the active screen
    lv_obj_clean(scr);                                        // Clear the screen (removes all objects)
    lv_obj_set_style_bg_color(scr, color, LV_PART_MAIN);      // Set the background color
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN); // Ensure full opacity
}

const int AVERAGE_VALUES      = 10;
/*
int GetMovingAverage(int newValue){
    static int values[AVERAGE_VALUES] = {0};
    static int index = 0;
    static int count = 0;
    static double sum = 0;

    // Subtract the value being replaced
    sum -= values[index];

    // Insert the new value
    values[index] = newValue;
    sum += newValue;

    // Update index and count
    index = (index + 1) % AVERAGE_VALUES;
    if (count < AVERAGE_VALUES) count++;

    float avg = sum / count;

    return (int)roundf(avg * 10.0f);
}
*/
// Function to handle touch input and draw black points on the screen
void touch_event_handler(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_get_act(); // Get the active input device
    if (indev == NULL) return; // Check if the input device is valid

    lv_point_t touch_point;
    lv_indev_get_point(indev, &touch_point); // Get the touch point

    // Ensure touch points are within screen boundaries
    if (touch_point.x >= 0 && touch_point.x < 800 && touch_point.y >= 0 && touch_point.y < 1280)
    {
        // Create a small black square (pixel-like) at the touch point
        lv_obj_t *pixel = lv_obj_create(lv_scr_act());
        lv_obj_set_size(pixel, 8, 8);                                   // Set the size for a larger pixel effect
        lv_obj_set_style_radius(pixel, 0, LV_PART_MAIN);                // No rounding for sharp square corners
        lv_obj_set_style_bg_color(pixel, LV_COLOR_BLACK, LV_PART_MAIN); // Set color to black
        lv_obj_set_style_bg_opa(pixel, LV_OPA_COVER, LV_PART_MAIN);     // Set opacity to fully opaque
        lv_obj_set_pos(pixel, touch_point.x, touch_point.y);    // Center the square at the touch point
    }
}

// Function to handle touch area events
void touch_area_event_handler(lv_event_t *e)
{
    // Prevent touch propagation and handle events here
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSING || code == LV_EVENT_PRESSED)
    {
        touch_event_handler(e); // Call the touch event handler
        lv_event_stop_processing(e); // Stop event propagation to prevent scrolling
    }
}


void demo_Waveshare(){
// if (disp != NULL)
    // {
    //     bsp_display_rotate(disp, LV_DISPLAY_ROTATION_90); // 90、180、270
    // }
   

    // Fill screen with red, green, and blue in sequence
    fill_screen_with_color(LV_COLOR_RED);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    fill_screen_with_color(LV_COLOR_GREEN);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    fill_screen_with_color(LV_COLOR_BLUE);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    // Fill the screen with white after blue
    fill_screen_with_color(LV_COLOR_WHITE);

    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello, World!");
    lv_obj_center(label);
}
bool InitialMessage(void){
    twai_message_t message;
if (twai_receive(&message, pdMS_TO_TICKS(3000)) == ESP_OK) {
    printf("Message received\n");\
    return true;
} else {
    printf("Failed to receive message\n");
    return false;
}
}

bool CANSetup(void){
    
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_4, TWAI_MODE_NORMAL); // Set to no-acknowledgment mode for testing
    g_config.mode = TWAI_MODE_NORMAL; // Set to normal mode for standard operation

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); //Potentially TWAI_TIMING_CONFIG_500KBITS
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); //May need to be changed to reduce data overload

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        printf("Driver installed\n");
    } else {
        printf("Failed to install driver\n");
        return false;
    }

    //Start TWAI driver
    if (twai_start() == ESP_OK) {
        printf("Driver started\n");
            return true;
    } else {
        printf("Failed to start driver\n");
        return false;
    }
    // if(InitialMessage() == true){
    //         return true;
    //     }
    return true;

}

static void ReceiveMessage(void *arg){
    while(1){
//Wait for message to be received
twai_message_t message;
esp_err_t err = twai_receive(&message, pdMS_TO_TICKS(10000));

if (err == ESP_OK) {
    printf("Message received\n");
    
} else if(err == ESP_ERR_TIMEOUT) {
    printf("Message Timeout\n");
    continue; // Skip processing if no message is received
    }
     else if(err == ESP_ERR_INVALID_ARG) {
    printf("Invalid argument\n");
    continue; // Skip processing if no message is received
    }
     else if(err == ESP_ERR_INVALID_STATE) {
    printf("Invalid state\n");
    continue; // Skip processing if no message is received
    }
     else if(err == ESP_FAIL) {
    printf("Failed to receive message\n");
    continue; // Skip processing if no message is received
    }



//Process received message
if (message.extd) {
    printf("Message is in Extended Format\n");
} else {
    printf("Message is in Standard Format\n");
}
printf("ID is %d\n", message.identifier);
if (!(message.rtr)) {
    printf("DLC = %d\n",message.data_length_code);
    for (int i = 0; i < message.data_length_code; i++) {
        printf("Data byte %d = %d\n", i, message.data[i]);
    }
}
}
}

uint32_t getNextCanId() {
  static uint8_t index = 0;
  uint32_t baseId = 0x100;
  uint32_t id = baseId + index;
  index = (index + 1) % 10;  // Cycle back to 0 after 9
  return id;
}

static void SendMessage(void *arg)
{
    while(1){
twai_message_t message;
message.identifier = 0xAAAA;
message.extd = 1;
message.data_length_code = 4;
for (int i = 0; i < 4; i++) {
    message.data[i] = 0;
}

esp_err_t err = twai_transmit(&message, pdMS_TO_TICKS(3000));
//Queue message for transmission
if (err == ESP_OK) {
    printf("Message queued for transmission\n");
} else if (err == ESP_ERR_TIMEOUT) {    
    printf("Failed on Timeout\n");
}
else if (err == ESP_ERR_INVALID_ARG) {
    printf("Invalid argument\n");
}
else if (err == ESP_FAIL) {
    printf("Driver not installed or stopped\n");
}
}
}


// ---- LVGL SD Filesystem Driver ----
static void *lvgl_fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode) {
    // const char *mode_str = (mode == LV_FS_MODE_WR) ? "wb" :
    //                        (mode == LV_FS_MODE_RD) ? "rb" : "rb+";
    // char full_path[256];
    // snprintf(full_path, sizeof(full_path), "%s/%s", MOUNT_POINT, path);
    // return (void *)fopen(full_path, mode_str);

        const char *clean_path = (path[0] == '/') ? path + 1 : path;
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", MOUNT_POINT, clean_path);
    
    #if DEBUG_DASH
    ESP_LOGI(TAG, "LVGL opening: %s", full_path);   // ← add this
    #endif
    
    FILE *f = fopen(full_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "fopen failed for: %s", full_path);  // ← and this
    }

    
    return (void *)f;
}

static lv_fs_res_t lvgl_fs_close(lv_fs_drv_t *drv, void *file_p) {
    fclose((FILE *)file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t lvgl_fs_read(lv_fs_drv_t *drv, void *file_p,
                                 void *buf, uint32_t btr, uint32_t *br) {
    *br = fread(buf, 1, btr, (FILE *)file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t lvgl_fs_seek(lv_fs_drv_t *drv, void *file_p,
                                 uint32_t pos, lv_fs_whence_t whence) {
    int w = (whence == LV_FS_SEEK_SET) ? SEEK_SET :
            (whence == LV_FS_SEEK_CUR) ? SEEK_CUR : SEEK_END;
    fseek((FILE *)file_p, pos, w);
    return LV_FS_RES_OK;
}

static lv_fs_res_t lvgl_fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p) {
    *pos_p = (uint32_t)ftell((FILE *)file_p);
    return LV_FS_RES_OK;
}

void lvgl_sd_fs_init(void) {
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);
    drv.letter   = 'S';
    drv.open_cb  = lvgl_fs_open;
    drv.close_cb = lvgl_fs_close;
    drv.read_cb  = lvgl_fs_read;
    drv.seek_cb  = lvgl_fs_seek;
    drv.tell_cb  = lvgl_fs_tell;
    lv_fs_drv_register(&drv);
    ESP_LOGI(TAG, "LVGL SD filesystem driver registered");
}


void Setup(void){
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
            .sw_rotate = false, // must be set to true for software rotation
        }};
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    
    
    lv_image_cache_resize(1024*1024*4,false);

    
    //SD CARD Initialization
  esp_err_t ret;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    #ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");
    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.

    ESP_LOGI(TAG, "Using SDMMC peripheral");
    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

     sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    lvgl_sd_fs_init();
    
}

static void gear_display_update(lv_obj_t *label, Gear gear) {
    switch (gear) {
        case GEAR_REVERSE: lv_label_set_text(label, "R"); break;
        case GEAR_NEUTRAL: lv_label_set_text(label, "N"); break;
        case GEAR_1:       lv_label_set_text(label, "1"); break;
        case GEAR_2:       lv_label_set_text(label, "2"); break;
        case GEAR_3:       lv_label_set_text(label, "3"); break;
        case GEAR_4:       lv_label_set_text(label, "4"); break;
        case GEAR_5:       lv_label_set_text(label, "5"); break;
        case GEAR_6:       lv_label_set_text(label, "6"); break;
        default:           lv_label_set_text(label, "-"); break;
    }
}

static void gear_display_update_anim(void *label, int32_t gear) {
    switch (gear) {
        case 7: lv_label_set_text((lv_obj_t *)label, "R"); break;
        case 0: lv_label_set_text((lv_obj_t *)label, "N"); break;
        case 1:       lv_label_set_text((lv_obj_t *)label, "1"); break;
        case 2:       lv_label_set_text((lv_obj_t *)label, "2"); break;
        case 3:       lv_label_set_text((lv_obj_t *)label, "3"); break;
        case 4:       lv_label_set_text((lv_obj_t *)label, "4"); break;
        case 5:       lv_label_set_text((lv_obj_t *)label, "5"); break;
        case 6:       lv_label_set_text((lv_obj_t *)label, "6"); break;
        default:           lv_label_set_text(label, "-"); break;
    }
}

static void set_angle(void * img, int32_t v)
{
    lv_image_set_rotation(img, v);
    //lv_obj_invalidate((lv_obj_t *)img);   // ← already just the needle if called on needle_img

    //printf("anim: %ld\n", v);
}
static void ui_set_speed(void * label, int32_t speed)
{
    lv_label_set_text_fmt((lv_obj_t *)label, "%03d", speed);
    //lv_obj_invalidate((lv_obj_t *)label);   // ← already just the needle if called on needle_img
}

static void set_fuel_level_anim(void *arc, int32_t value) {
    lv_arc_set_value((lv_obj_t *)arc, value);
}

//TODO CREATE FUEL ARC AND ANIMATION
static void CreateFuelArc(int32_t x_offset, int32_t y_offset) {
    fuel_arc = lv_arc_create(lv_scr_act());

    // Match your fuel gauge graphic size and position
    lv_obj_set_size(fuel_arc, 730, 695);
    lv_obj_align(fuel_arc, LV_ALIGN_CENTER, x_offset, y_offset);

    // Arc sweeps from 180° (9 o'clock, empty) to 360° (3 o'clock, full)
    lv_arc_set_bg_angles(fuel_arc, 195, 320);
    lv_arc_set_range(fuel_arc, 0, 10);
    lv_arc_set_value(fuel_arc, 10);

    // Hide background track
    lv_obj_set_style_arc_opa(fuel_arc, LV_OPA_TRANSP, LV_PART_MAIN);

    // Match your PNG arc width — 40px arc width needs padding adjustment
    lv_obj_set_style_arc_width(fuel_arc, 30, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(fuel_arc, false, LV_PART_INDICATOR);  // flat ends to match PNG
    lv_obj_set_style_arc_color(fuel_arc, lv_color_make(0, 165, 255), LV_PART_INDICATOR);

    // Pull arc inward so its centre radius matches your PNG
    // arc sits at outer edge by default — pad inward by half arc width
    lv_obj_set_style_pad_all(fuel_arc, 15, LV_PART_MAIN);  // half of 40px width

    // Hide knob
    lv_obj_set_style_bg_opa(fuel_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(fuel_arc, 0, LV_PART_KNOB);

    // Non interactive
    lv_obj_remove_flag(fuel_arc, LV_OBJ_FLAG_CLICKABLE);

    // Layer order — above canvas, below needle
    lv_obj_move_foreground(fuel_arc);
    lv_obj_move_foreground(needle_img);
}

static void CreateAnimation(void * img, lv_anim_exec_xcb_t change, int32_t start, int32_t end, bool test){
    lv_anim_delete(img, change);   // ← kill any existing animation on this object

     lv_anim_t anim; 
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, img);
    lv_anim_set_exec_cb(&anim, change);
    lv_anim_set_values(&anim, start, end);

    if(test){
    lv_anim_set_duration(&anim, 5000);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    }
    lv_anim_start(&anim);
}

static void DrawImage(lv_obj_t ** img, const void *imgSrc){
    *img = lv_image_create(lv_scr_act());
      if (lv_image_src_get_type(imgSrc) == LV_IMAGE_SRC_FILE) {
        lv_image_set_src(*img, (const char *)imgSrc);
    } else {
        lv_image_set_src(*img, imgSrc);
    }
    
    lv_obj_set_size(*img,800,800);
    lv_obj_center(*img);

    lv_obj_clear_flag(*img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(*img, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_flag(*img, LV_OBJ_FLAG_HIDDEN);  // ← start hidden, canvas reveals result

}

static void DrawFuelImage(lv_obj_t ** img, const void *imgSrc){
      //lv_obj_clear_flag(fuel_img, LV_OBJ_FLAG_HIDDEN);

    *img = lv_image_create(lv_scr_act());
      if (lv_image_src_get_type(imgSrc) == LV_IMAGE_SRC_FILE) {
        lv_image_set_src(*img, (const char *)imgSrc);
    } else {
        lv_image_set_src(*img, imgSrc);
    }
    
    lv_obj_set_size(*img,800,800);
    lv_obj_center(*img);

    lv_obj_clear_flag(*img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(*img, LV_OBJ_FLAG_SCROLLABLE);

    //lv_obj_add_flag(*img, LV_OBJ_FLAG_HIDDEN);  // ← start hidden, canvas reveals result
}

static void DrawNeedle(lv_obj_t ** img, const void *imgSrc){
  *img = lv_image_create(lv_scr_act());
      if (lv_image_src_get_type(imgSrc) == LV_IMAGE_SRC_FILE) {
        lv_image_set_src(*img, (const char *)imgSrc);
    } else {
        lv_image_set_src(*img, imgSrc);
    }
    lv_obj_align(*img,LV_ALIGN_CENTER,0, 160);
    //lv_obj_center(*img);
    lv_image_set_pivot(*img, 40, -20);


}

static void DrawLabel(lv_style_t *labelStyle, lv_obj_t **label, const char *initialText, int32_t xOff, int32_t yOff, bool animated){
    *label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_decor(*label,LV_TEXT_DECOR_NONE, LV_PART_MAIN);
    lv_label_set_text(*label, initialText);   // initial value
    lv_obj_align(*label, LV_ALIGN_CENTER, xOff, yOff);
    lv_obj_add_style(*label, labelStyle,LV_PART_MAIN);

    if(animated)
    {
        CreateAnimation(*label,ui_set_speed, 0, 250, true);
    }
}


static void FlattenStaticLayers(lv_color_t face_tint, lv_opa_t face_tint_opa, lv_color_t mid_tint, lv_opa_t mid_tint_opa, bool retint) { 
     if (static_canvas != NULL) {
        lv_obj_delete(static_canvas);
        lv_obj_clear_flag(face_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(mid_img,  LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(hud_img,  LV_OBJ_FLAG_HIDDEN);
        static_canvas = NULL;
    }

    static uint8_t *canvas_buf = NULL;
    size_t buf_size = 800 * 800 * 4;

    if (!canvas_buf) {
        canvas_buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
        if (!canvas_buf) {
            ESP_LOGE(TAG, "Failed to allocate canvas buffer");
            return;
        }
    }
    memset(canvas_buf, 0, buf_size);   // ← clear before redraw


    static_canvas = lv_canvas_create(lv_scr_act());
    lv_canvas_set_buffer(static_canvas, canvas_buf, 800, 800,
                         LV_COLOR_FORMAT_ARGB8888);
    lv_obj_center(static_canvas);

    lv_layer_t layer;
    lv_canvas_init_layer(static_canvas, &layer);

    // ---- Fill background colour first ----
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = LV_BACK_COLOR;
    rect_dsc.bg_opa   = LV_OPA_COVER;
    rect_dsc.radius   = 0;
    lv_area_t area    = {0, 0, 799, 799};
    lv_draw_rect(&layer, &rect_dsc, &area);

    // ---- Draw images on top ----
    lv_draw_image_dsc_t img_dsc;
    lv_draw_image_dsc_init(&img_dsc);
    if(retint){
    img_dsc.recolor     = face_tint;
    img_dsc.recolor_opa = face_tint_opa;
    }
   

    img_dsc.src = &ram_background.dsc;
    lv_draw_image(&layer, &img_dsc, &area);
    
    if(retint){
    img_dsc.recolor     = mid_tint;
    img_dsc.recolor_opa = mid_tint_opa;
    }

    img_dsc.src = &ram_middle.dsc;
    lv_draw_image(&layer, &img_dsc, &area);

    img_dsc.recolor     = lv_color_hex(0xCECECE);
    img_dsc.recolor_opa = LV_OPA_50;

    img_dsc.src = &ram_hud.dsc;
    lv_draw_image(&layer, &img_dsc, &area);

    //img_dsc.src = &ram_fuel.dsc;
    //lv_draw_image(&layer, &img_dsc, &area);

    lv_canvas_finish_layer(static_canvas, &layer);

    lv_obj_add_flag(face_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mid_img,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(hud_img,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(static_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(static_canvas, LV_OBJ_FLAG_SCROLLABLE);
    //lv_obj_move_foreground(needle_img);
}




void DrawDash(char * facePath, char * midPath, char * needlePath, lv_color_t face_tint, lv_opa_t face_tint_opa, lv_color_t mid_tint, lv_opa_t mid_tint_opa, bool retint) {
    ESP_LOGI(TAG, "DrawDash called");   // if this prints twice, something is calling it twice


    // Add this at the very start of DrawDash before anything else
ESP_LOGI(TAG, "Free PSRAM: %zu", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
ESP_LOGI(TAG, "Free internal RAM: %zu", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

        // Load from SD into PSRAM once
    ESP_ERROR_CHECK(image_load_from_sd(&ram_background, facePath));
    ESP_ERROR_CHECK(image_load_from_sd(&ram_middle,     midPath));
    ESP_ERROR_CHECK(image_load_from_sd(&ram_needle,     needlePath));
    ESP_ERROR_CHECK(image_load_from_sd(&ram_hud,     "S:general/HUD.bin"));
    ESP_ERROR_CHECK(image_load_from_sd(&ram_fuel,     "S:general/FUELfull.bin"));


    // Pass descriptor pointer exactly like a header image — SD never touched again
    DrawImage(&face_img, &ram_background.dsc);
    DrawImage(&mid_img,     &ram_middle.dsc);
    DrawImage(&hud_img, &ram_hud.dsc);
    DrawImage(&fuel_img, &ram_fuel.dsc);
    DrawNeedle(&needle_img,    &ram_needle.dsc);
    
    CreateAnimation(needle_img, set_angle, 0,2400,true);


    FlattenStaticLayers(face_tint, face_tint_opa, mid_tint, mid_tint_opa, retint); // 0xC8C566 //0xFF0064

    CreateFuelArc(-5, -60);   // shift upward into top quadrants
    CreateAnimation(fuel_arc, set_fuel_level_anim, 0, 10, true);
    
    
    DrawFuelImage(&fuel_img, &ram_fuel.dsc);
  

    static lv_style_t labelStyle;
    lv_style_init(&labelStyle);
    lv_style_set_text_font(&labelStyle, &helvetica_60);

    static lv_style_t labelStyle2;
    lv_style_init(&labelStyle2);
    lv_style_set_text_font(&labelStyle2, &helvetica_B_I_70);

     static lv_style_t labelStyle3;
    lv_style_init(&labelStyle3);
    lv_style_set_text_font(&labelStyle3, &helvetica_B_I_90);
    
    DrawLabel(&labelStyle2, &gear_label,"N",240,-40,false);
    DrawLabel(&labelStyle3, &speed_label,"000",240,80,true);
    DrawLabel(&labelStyle,&unit_label,"KM\x2FH",240,160,false);


    //TEST GEAR CHANGE
    CreateAnimation(gear_label,gear_display_update_anim, 0, 7, true);
}

// Call on theme change — only updates sources and reflattens, no new objects
void DrawDash_Update(const char *facePath, const char *midPath, const char *needlePath,
                     lv_color_t face_tint, lv_opa_t face_tint_opa,
                     lv_color_t mid_tint,  lv_opa_t mid_tint_opa, bool retint) {

    // Update image sources on existing objects
    lv_image_set_src(face_img,   &ram_background.dsc);
    lv_image_set_src(mid_img,    &ram_middle.dsc);
    lv_image_set_src(needle_img, &ram_needle.dsc);

    // Reflatten with new tint — reuses existing canvas buffer
    FlattenStaticLayers(face_tint, face_tint_opa, mid_tint, mid_tint_opa, retint);

    // Bring needle and fuel image back to front
    lv_obj_move_foreground(fuel_arc);
    lv_obj_move_foreground(fuel_img);
    lv_obj_move_foreground(needle_img);
    lv_obj_move_foreground(speed_label);
    lv_obj_move_foreground(gear_label);
    lv_obj_move_foreground(unit_label);
}




void run_anim(){
    lv_anim_t anim; 
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, &needle_img);
    lv_anim_set_exec_cb(&anim, set_angle);
    lv_anim_set_values(&anim, 0, 3600);
    lv_anim_set_duration(&anim, 5000);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);
}



static void on_menu_apply(const GaugeFace *face, const ThemeColor *color)
{
    ESP_LOGI("APPLY", "Face: %s, Color: %s", face->name, color->name);
    vTaskSuspend(TWAI_Recieve_Task);
    bsp_display_lock(0);

    // --- Unload old face images from PSRAM ---
    image_unload(&ram_background);
    image_unload(&ram_middle);
    image_unload(&ram_needle);
    //image_unload(&ram_hud);
    //image_unload(&ram_fuel);

    // --- Load new face images ---
    if (image_load_from_sd(&ram_background, face->face_bin)   != ESP_OK ||
        image_load_from_sd(&ram_middle,     face->mid_bin)    != ESP_OK ||
        image_load_from_sd(&ram_needle,     face->needle_bin) != ESP_OK)
    {
        ESP_LOGE("MENU_APPLY", "Failed to load new face images – reverting not implemented");
        bsp_display_unlock();
        return;
    }

    // // Update LVGL image sources (already drawn objects, just change src)
    // lv_image_set_src(face_img,   &ram_background.dsc);
    // lv_image_set_src(mid_img,    &ram_middle.dsc);
    // lv_image_set_src(needle_img, &ram_needle.dsc);

    //lv_anim_del(needle_img, set_angle); // Stop any existing needle animation
    //lv_anim_del(fuel_arc, set_fuel_level_anim); // Stop any existing fuel arc animation
    //lv_anim_del(gear_label, gear_display_update_anim); // Stop any existing gear label animation
    //lv_anim_del(speed_label, ui_set_speed); // Stop any existing speed label animation


    DrawDash_Update(face->face_bin, face->mid_bin, face->needle_bin,
              color->face_tint, color->face_tint_opa,
              color->mid_tint,  color->mid_tint_opa, color->retint
            );
    // FlattenStaticLayers(color->face_tint, color->face_tint_opa,
    //                     color->mid_tint,  color->mid_tint_opa);
    // // Re-flatten static layers with new tint
    //FlattenStaticLayers(color->face_tint, color->face_tint_opa,color->mid_tint,  color->mid_tint_opa);

    bsp_display_unlock();
    vTaskResume(TWAI_Recieve_Task);
    ESP_LOGI("MENU_APPLY", "Applied face=%s", face->name);
}

static void dash_longpress_cb(lv_event_t *e)
{
    menu_show();
}


void app_main(void)
{
    Setup();
  



    //fill_screen_with_color(LV_COLOR_WHITE);
    /*
    // Leave Touch Area here for now so that I have event handler ready
    // Create an enlarged touch area to capture all touch events
    lv_obj_t *touch_area = lv_obj_create(lv_scr_act());
    lv_obj_set_size(touch_area, 840, 1320); // Set size larger than the screen (adjust as needed)
    lv_obj_align(touch_area, LV_ALIGN_CENTER, 0, 0); // Center it on the screen

    // Make the touch area invisible but still capture touch events
    lv_obj_set_style_bg_opa(touch_area, LV_OPA_TRANSP, LV_PART_MAIN); // Make it transparent

    // Register the touch area event handler
    lv_obj_add_event_cb(touch_area, touch_area_event_handler, LV_EVENT_ALL, NULL);
    */
    bsp_display_lock(0);
    //DrawTest();
    menu_init(on_menu_apply);  // make sure this line exists and on_menu_apply is not NULL

    DrawDash("S:themes/tabby/face.bin", "S:themes/tabby/mid.bin", "S:themes/tabby/needle.bin",lv_color_hex(0x313131),LV_OPA_50,lv_color_hex(0x313131), LV_OPA_50,false);
    // Register long-press on the screen to open the menu
    lv_obj_add_event_cb(lv_scr_act(), dash_longpress_cb, LV_EVENT_LONG_PRESSED, NULL);
    bsp_display_unlock();
        while (countTry < 1){
        if(CANSetup() == true)
        {
        xTaskCreatePinnedToCore(ReceiveMessage, "TWAI_Recieve", 8192, NULL, RX_TASK_PRIO, &TWAI_Recieve_Task, 0); //PIN TO CORE 0 and increase stack size to 8192
        //xTaskCreatePinnedToCore(SendMessage, "TWAI_Send", 4096, NULL, RX_TASK_PRIO, NULL, tskNO_AFFINITY);

        break; 
        }
        else{
        ESP_LOGI(TAG,"Retrying CANBUS Initialization Count = %d", countTry);
        countTry++;
        }
    
    }

    
    while (true)
    {
        // Let LVGL handle rendering and input
        //vTaskDelay(portMAX_DELAY);
        //lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(500));
       //printf("TEST");
        //lv_task_handler(); // Call LVGL's task handler periodically

       
    }
}
