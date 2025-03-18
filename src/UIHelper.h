#ifndef UI_HELPER_H
#define UI_HELPER_H

#include <Arduino.h>
#include <lvgl.h>
#include <M5Unified.h>
#include "Config.h"

// Screen dimensions for CoreS3
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// UI components
extern lv_obj_t* battery_icon;
extern lv_obj_t* battery_label;
extern lv_obj_t* wifi_label;
extern lv_obj_t* time_label;
extern lv_obj_t* status_bar;

// Scrollable object for gesture handling
extern lv_obj_t* current_scroll_obj;

// Styles for UI
extern lv_style_t style_screen;
extern lv_style_t style_btn;
extern lv_style_t style_btn_pressed;
extern lv_style_t style_title;
extern lv_style_t style_text;
extern lv_style_t style_card_action;
extern lv_style_t style_card_info;
extern lv_style_t style_card_pressed;
extern lv_style_t style_keyboard_btn;

// Functions
void initStyles();
void addStatusBar(lv_obj_t* screen);
void updateStatus(const char* message, uint32_t color);
void addTimeDisplay(lv_obj_t* screen);
void updateTimeDisplay();
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
void handleSwipeLeft();
void handleSwipeVertical(int amount);

// Debug macros - only include if not defined in Config.h
#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED true
#define DEBUG_PRINT(x) if(DEBUG_ENABLED) { Serial.print(millis()); Serial.print(": "); Serial.println(x); }
#define DEBUG_PRINTF(x, ...) if(DEBUG_ENABLED) { Serial.print(millis()); Serial.print(": "); Serial.printf(x, __VA_ARGS__); }
#endif

#endif // UI_HELPER_H