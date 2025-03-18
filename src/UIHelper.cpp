#include "UIHelper.h"

// UI components
lv_obj_t* battery_icon = nullptr;
lv_obj_t* battery_label = nullptr;
lv_obj_t* wifi_label = nullptr;
lv_obj_t* time_label = nullptr;
lv_obj_t* status_bar = nullptr;

// Styles for UI
lv_style_t style_screen;
lv_style_t style_btn;
lv_style_t style_btn_pressed;
lv_style_t style_title;
lv_style_t style_text;
lv_style_t style_card_action;
lv_style_t style_card_info;
lv_style_t style_card_pressed;
lv_style_t style_keyboard_btn;

// Global variables for scrolling
lv_obj_t *current_scroll_obj = nullptr;
const int SCROLL_AMOUNT = 40;  // Pixels to scroll per button press

// Touch tracking inspired by touch.ino
static bool touch_active = false;
static int16_t touch_start_x = 0;
static int16_t touch_start_y = 0;
static int16_t touch_last_x = 0;
static int16_t touch_last_y = 0;
static unsigned long touch_start_time = 0;
static bool was_touching = false;  // Add this line
const int TOUCH_SWIPE_THRESHOLD = 30;  // Pixels for a swipe
const int TOUCH_MAX_SWIPE_TIME = 700;  // Max time (ms) for a swipe

// Display flush function
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    M5.Display.startWrite();
    M5.Display.setAddrWindow(area->x1, area->y1, w, h);
    M5.Display.pushPixels((uint16_t *)color_p, w * h);
    M5.Display.endWrite();
    lv_disp_flush_ready(disp);
}

// Touchpad read function
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    lgfx::touch_point_t tp[1];
    static bool was_touching = false;
    static int16_t touch_start_x = 0;
    static int16_t touch_start_y = 0;
    static int16_t touch_last_x = 0;
    static int16_t touch_last_y = 0;
    static unsigned long touch_start_time = 0;

    M5.update();
    
    // Get touch points using the CoreS3 User Demo approach
    int nums = M5.Display.getTouchRaw(tp, 1);
    if (nums) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = tp[0].x;
        data->point.y = tp[0].y;
        
        if (!was_touching) {
            touch_start_x = tp[0].x;
            touch_start_y = tp[0].y;
            touch_start_time = millis();
            was_touching = true;
        }
        touch_last_x = tp[0].x;
        touch_last_y = tp[0].y;
    } else if (was_touching) {
        data->state = LV_INDEV_STATE_REL;
        was_touching = false;

        int dx = touch_last_x - touch_start_x;
        int dy = touch_last_y - touch_start_y;
        int abs_dx = abs(dx);
        int abs_dy = abs(dy);
        unsigned long duration = millis() - touch_start_time;

        if (duration < TOUCH_MAX_SWIPE_TIME) {
            if (abs_dx > TOUCH_SWIPE_THRESHOLD && abs_dx > abs_dy) {
                if (dx < 0) handleSwipeLeft();
            } else if (abs_dy > TOUCH_SWIPE_THRESHOLD && abs_dy > abs_dx) {
                handleSwipeVertical(dy < 0 ? -SCROLL_AMOUNT : SCROLL_AMOUNT);
            }
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// Swipe left function - implementation depends on screens in main application
void handleSwipeLeft() {
    // This function will be implemented in the main application
    // as it depends on the application-specific screens
    DEBUG_PRINT("Swipe left detected");
}

// Swipe vertical function
void handleSwipeVertical(int amount) {
    if (current_scroll_obj && lv_obj_is_valid(current_scroll_obj)) {
        lv_obj_scroll_by(current_scroll_obj, 0, amount, LV_ANIM_ON);
        DEBUG_PRINTF("Scrolled object by %d pixels\n", amount);
        lv_obj_invalidate(current_scroll_obj);
    } else {
        DEBUG_PRINT("No valid scrollable object");
    }
}

// Initialize all UI styles
void initStyles() {
    // Screen style
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_hex(0x1A1A1A)); // Dark background
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);

    // Button style
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, lv_color_hex(0x4A90E2)); // Blue button
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn, lv_color_hex(0xFFFFFF)); // White text
    lv_style_set_border_width(&style_btn, 0);

    // Button pressed style
    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, lv_color_hex(0x357ABD)); // Darker blue when pressed
    lv_style_set_bg_opa(&style_btn_pressed, LV_OPA_COVER);

    // Title style
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_20);
    lv_style_set_text_color(&style_title, lv_color_hex(0xFFFFFF)); // White text

    // Card action style (for interactive cards)
    lv_style_init(&style_card_action);
    lv_style_set_bg_color(&style_card_action, lv_color_hex(0x2D2D2D)); // Card background
    lv_style_set_bg_opa(&style_card_action, LV_OPA_COVER);
    lv_style_set_border_color(&style_card_action, lv_color_hex(0x4A90E2)); // Blue border
    lv_style_set_border_width(&style_card_action, 1);

    // Card info style (for info display cards)
    lv_style_init(&style_card_info);
    lv_style_set_bg_color(&style_card_info, lv_color_hex(0x2D2D2D)); // Card background
    lv_style_set_bg_opa(&style_card_info, LV_OPA_COVER);
    lv_style_set_border_color(&style_card_info, lv_color_hex(0x666666)); // Gray border
    lv_style_set_border_width(&style_card_info, 1);
    lv_style_set_radius(&style_card_info, 5); // Rounded corners

    // Card pressed style
    lv_style_init(&style_card_pressed);
    lv_style_set_bg_color(&style_card_pressed, lv_color_hex(0x357ABD)); // Blue when pressed
    lv_style_set_bg_opa(&style_card_pressed, LV_OPA_COVER);

    // Text style
    lv_style_init(&style_text);
    lv_style_set_text_color(&style_text, lv_color_hex(0xFFFFFF)); // White text
    lv_style_set_text_font(&style_text, &lv_font_montserrat_14);

    // Keyboard button style
    lv_style_init(&style_keyboard_btn);
    lv_style_set_bg_color(&style_keyboard_btn, lv_color_hex(0x333333)); // Dark gray keys
    lv_style_set_text_color(&style_keyboard_btn, lv_color_hex(0xFFFFFF)); // White text
    lv_style_set_radius(&style_keyboard_btn, 5); // Rounded corners
    lv_style_set_border_width(&style_keyboard_btn, 1);
    lv_style_set_border_color(&style_keyboard_btn, lv_color_hex(0x444444)); // Darker border

    DEBUG_PRINT("Styles initialized");
}

// Add a status bar to a screen
void addStatusBar(lv_obj_t* screen) {
    if (status_bar) {
        lv_obj_del(status_bar);
        status_bar = nullptr;
    }
    
    status_bar = lv_label_create(screen);
    lv_obj_set_style_text_font(status_bar, &lv_font_montserrat_14, 0);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_text_color(status_bar, lv_color_hex(0xFFFFFF), 0); // White text
    lv_label_set_text(status_bar, "Ready");
}

// Update the status message and color
void updateStatus(const char* message, uint32_t color) {
    if (!status_bar) return;

    // Update status message
    lv_label_set_text(status_bar, message);
    lv_obj_set_style_text_color(status_bar, lv_color_hex(color), 0);
}

// Add a time display to a screen
void addTimeDisplay(lv_obj_t *screen) {
    lv_obj_t* time_card = lv_obj_create(screen);
    lv_obj_set_size(time_card, 180, 40);
    lv_obj_align(time_card, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(time_card, &style_card_info, 0);

    lv_obj_t* time_icon = lv_label_create(time_card);
    lv_label_set_text(time_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(time_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(time_icon, lv_color_hex(0x4A90E2), 0);
    lv_obj_align(time_icon, LV_ALIGN_LEFT_MID, 15, 0);

    time_label = lv_label_create(time_card);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_16, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 10, 0);

    updateTimeDisplay(); // Initial update
}

// Update the time display with current time
void updateTimeDisplay() {
    if (time_label == nullptr) return;

    m5::rtc_time_t TimeStruct;
    M5.Rtc.getTime(&TimeStruct);
    
    // Convert 24-hour to 12-hour format
    int hour = TimeStruct.hours;
    const char* period = (hour >= 12) ? "PM" : "AM";
    if (hour == 0) {
        hour = 12; // Midnight
    } else if (hour > 12) {
        hour -= 12;
    }

    char timeStr[24];
    snprintf(timeStr, sizeof(timeStr), "%d:%02d:%02d %s", 
             hour, TimeStruct.minutes, TimeStruct.seconds, period);
    lv_label_set_text(time_label, timeStr);
}