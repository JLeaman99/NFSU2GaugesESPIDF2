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



#include "driver/twai.h"
#include "driver/gpio.h"
#include "short_baby_60.h"

#include "tabby_needle.h"
#include "needle.h"
#include "tabby_tick.h"
#include "FUELfull.h"
#include "face.h"
#include "mid.h"
#include "total.h"

//CAN VARIABLES
#define CAN_TX_GPIO (gpio_num_t)5
#define CAN_RX_GPIO (gpio_num_t)4



LV_IMG_DECLARE(tabby_needle);
LV_IMG_DECLARE(tabby_tick);
LV_IMG_DECLARE(FUELfull);
LV_IMG_DECLARE(total);
LV_IMG_DECLARE(face);
LV_IMG_DECLARE(mid);
LV_IMG_DECLARE(needle);

LV_FONT_DECLARE(short_baby_60);

lv_obj_t *face_img, *mid_img, *needle_img, *fuel_img;
static lv_obj_t *speed_label, *unit_label;

// Define colors manually
#define LV_COLOR_RED lv_color_make(0xFF, 0x00, 0x00)
#define LV_BACK_COLOR lv_color_make(0x1b,0x1b,0x1b)
#define LV_COLOR_GREEN lv_color_make(0x00, 0xFF, 0x00)
#define LV_COLOR_BLUE lv_color_make(0x00, 0x00, 0xFF)
#define LV_COLOR_WHITE lv_color_make(0xFF, 0xFF, 0xFF)
#define LV_COLOR_BLACK lv_color_make(0x00, 0x00, 0x00) // Define black color



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

void CANSetup(void){
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO,CAN_RX_GPIO,TWAI_MODE_LISTEN_ONLY);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_1MBITS(); //Potentially TWAI_TIMING_CONFIG_500KBITS
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL(); //May need to be changed to reduce data overload

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        printf("Driver installed\n");
    } else {
        printf("Failed to install driver\n");
        return;
    }

    //Start TWAI driver
    if (twai_start() == ESP_OK) {
        printf("Driver started\n");
    } else {
        printf("Failed to start driver\n");
        return;
    }

}

void ReceiveMessage(void){
//Wait for message to be received
twai_message_t message;
if (twai_receive(&message, pdMS_TO_TICKS(10000)) == ESP_OK) {
    printf("Message received\n");
} else {
    printf("Failed to receive message\n");
    return;
}

//Process received message
if (message.extd) {
    printf("Message is in Extended Format\n");
} else {
    printf("Message is in Standard Format\n");
}
printf("ID is %d\n", message.identifier);
if (!(message.rtr)) {
    for (int i = 0; i < message.data_length_code; i++) {
        printf("Data byte %d = %d\n", i, message.data[i]);
    }
}
}

void SendMessage(uint8_t toSend[])
{

    // WILL NEED TO VERIFY THAT toSEND doesnt need to be a pointer and how to set message data
    //Configure message to transmit
twai_message_t message;
message.identifier = 0xAAAA;
message.extd = 1;
message.data_length_code = sizeof(toSend) / sizeof(toSend[0]);

for (int i = 0; i < message.data_length_code; i++) {
    message.data[i] = toSend[i];
}

//Queue message for transmission
if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    printf("Message queued for transmission\n");
} else {
    printf("Failed to queue message for transmission\n");
}
}

void Setup(void){
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false, // must be set to true for software rotation
        }};
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

}


static void set_angle(void * img, int32_t v)
{
    lv_image_set_rotation(img, v);
    //printf("anim: %ld\n", v);
}
static void ui_set_speed(void * label, int32_t speed)
{
    lv_label_set_text_fmt((lv_obj_t *)label, "%03d", speed);
}

void DrawTest(void){
fuel_img = lv_image_create(lv_scr_act());
lv_image_set_src(fuel_img, &FUELfull);
lv_obj_set_size(fuel_img, 800,800);
lv_obj_center(fuel_img);

face_img = lv_image_create(lv_scr_act());
lv_image_set_src(face_img, &face);
lv_obj_set_size(face_img, 800,800);
lv_obj_center(face_img);

mid_img = lv_image_create(lv_scr_act());
lv_image_set_src(mid_img, &mid);
lv_obj_set_size(mid_img, 800,800);
lv_obj_center(mid_img);

needle_img = lv_image_create(lv_scr_act());
lv_image_set_src(needle_img, &needle);
//lv_img_set_zoom(needle_img,512);
lv_obj_align(needle_img, LV_ALIGN_CENTER,0, 160);
lv_image_set_pivot(needle_img, 40, -20);
//lv_image_set_rotation(needle_img,1800);

lv_anim_t anim; 
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, needle_img);
    lv_anim_set_exec_cb(&anim, set_angle);
    lv_anim_set_values(&anim, 0, 2400);
    lv_anim_set_duration(&anim, 5000);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);

static lv_style_t labelStyle;
lv_style_init(&labelStyle);
lv_style_set_text_font(&labelStyle, &short_baby_60);


speed_label = lv_label_create(lv_scr_act());
    lv_label_set_text(speed_label, "000");   // initial value
    lv_obj_align(speed_label, LV_ALIGN_CENTER, 200, 80);
    lv_obj_add_style(speed_label, &labelStyle,LV_PART_MAIN);
    
    lv_anim_t speed_anim; 
    lv_anim_init(&speed_anim);
    lv_anim_set_var(&speed_anim, speed_label);
    lv_anim_set_exec_cb(&speed_anim, ui_set_speed);
    lv_anim_set_values(&speed_anim, 0, 250);
    lv_anim_set_duration(&speed_anim, 5000);
    lv_anim_set_repeat_count(&speed_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&speed_anim);


    unit_label = lv_label_create(lv_scr_act());
    lv_label_set_text(unit_label, "KPH");   // initial value
    lv_obj_align(unit_label, LV_ALIGN_CENTER, 200, 140);
    lv_obj_add_style(unit_label, &labelStyle,LV_PART_MAIN);
    
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
    fill_screen_with_color(LV_BACK_COLOR);

    lvgl_port_lock(0);
    DrawTest();


  
    // Disable scrolling for the entire screen
    //lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    //lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    //lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLL_CHAIN_VER);

    
    static lv_style_t recolor_style;
    lv_style_init(&recolor_style);
    lv_style_set_image_recolor(&recolor_style, lv_color_hex(0xFF0064)); // Red tint
    lv_style_set_image_recolor_opa(&recolor_style, LV_OPA_50);          // 80% opacity

    lv_obj_add_style(mid_img,&recolor_style,LV_PART_MAIN);

   /* static lv_style_t recolor_style2;
    lv_style_init(&recolor_style2);
    lv_style_set_image_recolor(&recolor_style2, lv_color_hex(0xffffff)); // yellow? tint
    lv_style_set_image_recolor_opa(&recolor_style2, LV_OPA_50);          // 80% opacity
    lv_obj_add_style(face_img,&recolor_style2,LV_PART_MAIN); 
*/
    

 // int needle_center_shift = 0; // how far the center of the needle is shifted from the left edge

  //lv_obj_align(needle_img, LV_ALIGN_CENTER, 108 - needle_center_shift, 0);
  //lv_image_set_pivot(needle_img, needle_center_shift, 36);

  //lv_obj_set_style_image_recolor_opa(needle_img, 255, 0);
    //run_anim();
  lvgl_port_unlock();
    
    while (true)
    {
        // Let LVGL handle rendering and input
        //vTaskDelay(portMAX_DELAY);
        //lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
       //printf("TEST");
        //lv_task_handler(); // Call LVGL's task handler periodically

       
    }
}
