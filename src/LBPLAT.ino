#include <M5Unified.h>
#include <lvgl.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>

// Include our modular components
#include "Config.h"
#include "WiFiManager.h"
#include "FileManager.h"
#include "UIHelper.h"

// Custom SPI instance for SD card
SPIClass SPI_SD;

// Global objects
WiFiManager wifiManager;
FileManager fileManager(SPI_SD);

// LVGL display and input drivers
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

// LVGL semaphore for thread safety
SemaphoreHandle_t xGuiSemaphore = nullptr;

// Display settings
uint8_t displayBrightness = 128; // Current brightness (0-255)

// Forward declarations for screens
void createMainMenu();
void createSettingsScreen();
void createWiFiManagerScreen();

// Forward declarations for date and time screens
static void createDateSelectionScreen();
static void createTimeSelectionScreen();
static void save_time_to_rtc();

// Static callback handlers for date and time selection
static void on_year_change(lv_event_t* e);
static void on_month_change(lv_event_t* e);
static void on_day_change(lv_event_t* e);
static void on_hour_change(lv_event_t* e);
static void on_minute_change(lv_event_t* e);

// Global variables to store selected time temporarily
static int selected_hour = 0, selected_minute = 0;

// Global variables to store selected date temporarily
static lv_calendar_date_t selected_date = {2025, 3, 15}; // Default date

// Static pointers to remember rollers between callbacks
static lv_obj_t* g_year_roller = nullptr;
static lv_obj_t* g_month_roller = nullptr;
static lv_obj_t* g_day_roller = nullptr;
static lv_obj_t* g_selected_date_label = nullptr;
static lv_obj_t* g_hour_roller = nullptr;
static lv_obj_t* g_minute_roller = nullptr;
static lv_obj_t* g_selected_time_label = nullptr;

// Screen references
lv_obj_t* main_menu_screen = nullptr;
lv_obj_t* settings_screen = nullptr;

void setup() {
  Serial.begin(115200);
  DEBUG_PRINT("Starting application...");

  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  cfg.output_power = true;
  cfg.internal_imu = true;
  cfg.internal_rtc = true;
  M5.begin(cfg);
  M5.Power.begin();

  // Enable external bus power
  M5.Power.setExtOutput(true);
  DEBUG_PRINT("External bus power enabled: " + String(M5.Power.getExtOutput() ? "Yes" : "No"));

  // Configure ALDO3 to 3.3V for external components
  M5.Power.Axp2101.writeRegister8(0x94, 28); // Set ALDO3 to 3.3V
  uint8_t reg90 = M5.Power.Axp2101.readRegister8(0x90);
  M5.Power.Axp2101.writeRegister8(0x90, reg90 | 0x08); // Enable ALDO3
  DEBUG_PRINT("ALDO3 configured to 3.3V");

  // Check AW9523 ports
  uint8_t port0_val = M5.In_I2C.readRegister8(AW9523_ADDR, 0x02, 100000);
  DEBUG_PRINTF("AW9523 PORT0: 0x%02X (BUS_EN: %d)", port0_val, (port0_val & 0x02) >> 1);
  uint8_t port1_val = M5.In_I2C.readRegister8(AW9523_ADDR, 0x03, 100000);
  DEBUG_PRINTF("AW9523 PORT1: 0x%02X (BOOST_EN: %d)", port1_val, (port1_val & 0x80) >> 7);

  // Monitor battery
  DEBUG_PRINTF("Battery Voltage: %d mV", M5.Power.getBatteryVoltage());
  DEBUG_PRINTF("VBUS Voltage: %d mV", M5.Power.getVBUSVoltage());

  // Initialize speaker
  M5.Speaker.begin();
  DEBUG_PRINT("Speaker initialized");

  // Load persistent settings
  Preferences prefs;
  prefs.begin("settings", false);
  uint8_t saved_volume = prefs.getUChar("volume", 128);
  bool sound_enabled = prefs.getBool("sound_enabled", true);
  M5.Speaker.setVolume(sound_enabled ? saved_volume : 0);
  DEBUG_PRINTF("Loaded sound settings - Enabled: %d, Volume: %d", sound_enabled, saved_volume);

  // Load display brightness
  displayBrightness = prefs.getUChar("brightness", 128);
  M5.Display.setBrightness(displayBrightness);
  DEBUG_PRINTF("Display brightness set to: %d", displayBrightness);
  prefs.end();

  // Initialize LVGL
  lv_init();

  // Initialize LVGL buffer
  static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(SCREEN_WIDTH * 40 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  static lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(SCREEN_WIDTH * 40 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_WIDTH * 40);
  DEBUG_PRINT("Display buffer initialized");

  // Initialize display driver
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = M5.Display.width();
  disp_drv.ver_res = M5.Display.height();
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  DEBUG_PRINT("Display driver registered");

  // Initialize input driver
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);
  DEBUG_PRINT("Input driver registered");

  // Create semaphore for LVGL thread safety
  xGuiSemaphore = xSemaphoreCreateMutex();

  // Set initial RTC time if unset
  m5::rtc_date_t DateStruct;
  M5.Rtc.getDate(&DateStruct);
  if (DateStruct.year < 2020) {
    // Set default date/time if RTC is unset
    DateStruct.year = 2025;
    DateStruct.month = 3;
    DateStruct.date = 15; // Default date
    DateStruct.weekDay = 5; // Thursday (simplified)
    M5.Rtc.setDate(&DateStruct);

    m5::rtc_time_t TimeStruct;
    TimeStruct.hours = 12;
    TimeStruct.minutes = 0;
    TimeStruct.seconds = 0;
    M5.Rtc.setTime(&TimeStruct);
    DEBUG_PRINT("RTC set to default: 2025-03-15 12:00:00");
  }
  
  // Set system time from RTC
  setSystemTimeFromRTC();

  // Initialize UI styles
  initStyles();

  // Initialize File Manager
  if (fileManager.begin()) {
    DEBUG_PRINT("File system initialized successfully");
  } else {
    DEBUG_PRINT("Failed to initialize file system");
  }

  // Initialize WiFi Manager with callbacks
  wifiManager.begin();
  wifiManager.setStatusCallback([](WiFiState state, const String& message) {
    DEBUG_PRINTF("WiFi Status: %s - %s", wifiManager.getStateString().c_str(), message.c_str());
    
    // Update status bar if visible
    if (status_bar != nullptr) {
      lv_label_set_text(status_bar, message.c_str());
      lv_obj_set_style_text_color(status_bar, 
        state == WiFiState::WIFI_CONNECTED ? lv_color_hex(0x00FF00) : 
        state == WiFiState::WIFI_CONNECTING ? lv_color_hex(0xFFFF00) : 
        lv_color_hex(0xFF0000), 0);
    }
  });
  
  wifiManager.setScanCallback([](const std::vector<NetworkInfo>& results) {
    DEBUG_PRINTF("WiFi scan complete, found %d networks", results.size());
    // Implement UI update with scan results
  });
  DEBUG_PRINT("WiFi Manager initialized");
  
  // Enable WiFi if needed
  wifiManager.setEnabled(true);

  // Create main menu screen
  createMainMenu();
  
  DEBUG_PRINT("Setup complete!");
}

// Set system time from RTC
void setSystemTimeFromRTC() {
  m5::rtc_date_t DateStruct;
  m5::rtc_time_t TimeStruct;
  M5.Rtc.getDate(&DateStruct);
  M5.Rtc.getTime(&TimeStruct);

  struct tm timeinfo = {0};
  timeinfo.tm_year = DateStruct.year - 1900; // Years since 1900
  timeinfo.tm_mon = DateStruct.month - 1;    // Months 0-11
  timeinfo.tm_mday = DateStruct.date;
  timeinfo.tm_hour = TimeStruct.hours;
  timeinfo.tm_min = TimeStruct.minutes;
  timeinfo.tm_sec = TimeStruct.seconds;
  timeinfo.tm_isdst = -1; // Let system handle DST if applicable

  time_t t = mktime(&timeinfo);
  struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
  settimeofday(&tv, NULL);
  DEBUG_PRINT("System time set from RTC");

  // Optional: Log current time for verification
  struct tm timeinfo_check;
  if (getLocalTime(&timeinfo_check)) {
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%A, %B %d %Y %I:%M:%S %p", &timeinfo_check);
    DEBUG_PRINTF("Current local time: %s", timeStr);
  }
}

// Main loop
void loop() {
  M5.update();
  uint32_t currentMillis = millis();
  
  // Update time display every second
  static unsigned long lastTimeUpdate = 0;
  if (currentMillis - lastTimeUpdate > 1000) {
    updateTimeDisplay();
    lastTimeUpdate = currentMillis;
  }

  // Handle LVGL timing
  static uint32_t lastLvglTick = 0;
  if (currentMillis - lastLvglTick > LV_TICK_PERIOD_MS) {
    if (xSemaphoreTake(xGuiSemaphore, (TickType_t)10) == pdTRUE) {
      lv_task_handler();
      xSemaphoreGive(xGuiSemaphore);
      lastLvglTick = currentMillis;
    }
  }

  // Update WiFi Manager
  wifiManager.update();

  // Battery status updates (every 5 seconds)
  static unsigned long lastBatteryUpdate = 0;
  if (currentMillis - lastBatteryUpdate > 5000) {
    if (battery_icon != nullptr && battery_label != nullptr) {
      int battery_level = M5.Power.getBatteryLevel();
      const char* battery_symbol = (battery_level > 75) ? LV_SYMBOL_BATTERY_FULL :
                                  (battery_level > 50) ? LV_SYMBOL_BATTERY_3 :
                                  (battery_level > 25) ? LV_SYMBOL_BATTERY_2 :
                                  (battery_level > 10) ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY;
      lv_label_set_text(battery_icon, battery_symbol);
      
      char battery_text[5];
      snprintf(battery_text, sizeof(battery_text), "%d%%", battery_level);
      lv_label_set_text(battery_label, battery_text);
    }
    
    if (wifi_label != nullptr && wifiManager.isConnected()) {
      int rssi = wifiManager.getRSSI();
      int wifi_strength = map(rssi, -100, -50, 0, 100);
      wifi_strength = constrain(wifi_strength, 0, 100);
      char wifi_text[5];
      snprintf(wifi_text, sizeof(wifi_text), "%d%%", wifi_strength);
      lv_label_set_text(wifi_label, wifi_text);
    }
    
    lastBatteryUpdate = currentMillis;
  }

  // Add a small delay for stability
  delay(5);
}

// Create main menu UI
void createMainMenu() {
  DEBUG_PRINT("Creating main menu");
  
  // Clean up existing screen if needed
  if (main_menu_screen != nullptr) {
    lv_obj_del(main_menu_screen);
    main_menu_screen = nullptr;
  }
  
  // Create new screen
  main_menu_screen = lv_obj_create(NULL);
  lv_obj_add_style(main_menu_screen, &style_screen, 0);

  // Header
  lv_obj_t* header = lv_obj_create(main_menu_screen);
  lv_obj_set_size(header, SCREEN_WIDTH, 40);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x2D2D2D), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "Loss Prevention");
  lv_obj_add_style(title, &style_title, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

  // Grid layout for menu cards
  lv_obj_t* grid = lv_obj_create(main_menu_screen);
  lv_obj_set_size(grid, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 50);
  lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, 50);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);
  
  static lv_coord_t col_dsc[] = {140, 140, LV_GRID_TEMPLATE_LAST}; // 2 columns
  static lv_coord_t row_dsc[] = {80, 80, 80, LV_GRID_TEMPLATE_LAST}; // 3 rows
  lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
  lv_obj_set_style_pad_column(grid, 10, 0);
  lv_obj_set_style_pad_row(grid, 10, 0);
  
  // Enable vertical scrolling
  lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_ACTIVE);
  lv_obj_set_scroll_snap_y(grid, LV_SCROLL_SNAP_NONE);
  lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  
  // Set as scrollable object for swipe gestures
  current_scroll_obj = grid;

  // Card 1: New Entry (Row 0, Col 0)
  lv_obj_t* new_card = lv_obj_create(grid);
  lv_obj_set_grid_cell(new_card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_add_style(new_card, &style_card_action, 0);
  lv_obj_add_style(new_card, &style_card_pressed, LV_STATE_PRESSED);
  
  lv_obj_t* new_icon = lv_label_create(new_card);
  lv_label_set_text(new_icon, LV_SYMBOL_PLUS);
  lv_obj_set_style_text_font(new_icon, &lv_font_montserrat_20, 0);
  lv_obj_align(new_icon, LV_ALIGN_TOP_MID, 0, 8);
  
  lv_obj_t* new_label = lv_label_create(new_card);
  lv_label_set_text(new_label, "New Entry");
  lv_obj_align(new_label, LV_ALIGN_BOTTOM_MID, 0, -8);

  // Card 2: Settings (Row 0, Col 1)
  lv_obj_t* settings_card = lv_obj_create(grid);
  lv_obj_set_grid_cell(settings_card, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_add_style(settings_card, &style_card_action, 0);
  lv_obj_add_style(settings_card, &style_card_pressed, LV_STATE_PRESSED);
  
  lv_obj_t* settings_icon = lv_label_create(settings_card);
  lv_label_set_text(settings_icon, LV_SYMBOL_SETTINGS);
  lv_obj_set_style_text_font(settings_icon, &lv_font_montserrat_20, 0);
  lv_obj_align(settings_icon, LV_ALIGN_TOP_MID, 0, 8);
  
  lv_obj_t* settings_label = lv_label_create(settings_card);
  lv_label_set_text(settings_label, "Settings");
  lv_obj_align(settings_label, LV_ALIGN_BOTTOM_MID, 0, -8);

  // Card 3: WiFi Manager (Row 1, Col 0)
  lv_obj_t* wifi_card = lv_obj_create(grid);
  lv_obj_set_grid_cell(wifi_card, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
  lv_obj_add_style(wifi_card, &style_card_action, 0);
  lv_obj_add_style(wifi_card, &style_card_pressed, LV_STATE_PRESSED);
  
  lv_obj_t* wifi_icon = lv_label_create(wifi_card);
  lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_20, 0);
  lv_obj_align(wifi_icon, LV_ALIGN_TOP_MID, 0, 8);
  
  lv_obj_t* wifi_mgr_label = lv_label_create(wifi_card);
  lv_label_set_text(wifi_mgr_label, "WiFi Manager");
  lv_obj_align(wifi_mgr_label, LV_ALIGN_BOTTOM_MID, 0, -8);
  
  // Add status bar
  addStatusBar(main_menu_screen);
  
  // Add time display
  addTimeDisplay(main_menu_screen);
  
  // Load the screen
  lv_scr_load(main_menu_screen);
}

// Create WiFi Manager screen
void createWiFiManagerScreen() {
  DEBUG_PRINT("Creating WiFi Manager screen");
  
  // Delete existing screens if needed
  if (main_menu_screen != nullptr) {
    lv_obj_del(main_menu_screen);
    main_menu_screen = nullptr;
  }
  
  // Create screen
  lv_obj_t* wifi_screen = lv_obj_create(NULL);
  lv_obj_add_style(wifi_screen, &style_screen, 0);
  
  // Header
  lv_obj_t* header = lv_obj_create(wifi_screen);
  lv_obj_set_size(header, SCREEN_WIDTH, 40);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x2D2D2D), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  
  // Back button
  lv_obj_t* back_btn = lv_btn_create(header);
  lv_obj_set_size(back_btn, 30, 30);
  lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_add_style(back_btn, &style_btn, 0);
  lv_obj_add_style(back_btn, &style_btn_pressed, LV_STATE_PRESSED);
  
  lv_obj_t* back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);
  
  lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
    createMainMenu();
  }, LV_EVENT_CLICKED, NULL);
  
  // Title
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "WiFi Manager");
  lv_obj_add_style(title, &style_title, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
  
  // WiFi enable/disable switch
  lv_obj_t* wifi_switch_label = lv_label_create(wifi_screen);
  lv_label_set_text(wifi_switch_label, "WiFi");
  lv_obj_align(wifi_switch_label, LV_ALIGN_TOP_LEFT, 15, 50);
  
  lv_obj_t* wifi_switch = lv_switch_create(wifi_screen);
  lv_obj_align(wifi_switch, LV_ALIGN_TOP_RIGHT, -15, 50);
  lv_obj_add_state(wifi_switch, wifiManager.isEnabled() ? LV_STATE_CHECKED : 0);
  
  lv_obj_add_event_cb(wifi_switch, [](lv_event_t* e) {
    lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    wifiManager.setEnabled(enabled);
    DEBUG_PRINTF("WiFi %s", enabled ? "enabled" : "disabled");
  }, LV_EVENT_VALUE_CHANGED, NULL);
  
  // WiFi status
  lv_obj_t* status_label = lv_label_create(wifi_screen);
  lv_label_set_text(status_label, "Status:");
  lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 15, 85);
  
  lv_obj_t* status_value = lv_label_create(wifi_screen);
  lv_label_set_text(status_value, wifiManager.getStateString().c_str());
  if (wifiManager.isConnected()) {
    lv_obj_set_style_text_color(status_value, lv_color_hex(0x00FF00), 0);
  } else if (wifiManager.getState() == WiFiState::WIFI_CONNECTING) {
    lv_obj_set_style_text_color(status_value, lv_color_hex(0xFFFF00), 0);
  } else {
    lv_obj_set_style_text_color(status_value, lv_color_hex(0xFF0000), 0);
  }
  lv_obj_align(status_value, LV_ALIGN_TOP_RIGHT, -15, 85);
  
  // WiFi network section
  lv_obj_t* network_label = lv_label_create(wifi_screen);
  lv_label_set_text(network_label, "Networks:");
  lv_obj_align(network_label, LV_ALIGN_TOP_LEFT, 15, 120);
  
  // Create scrollable container for networks
  lv_obj_t* network_container = lv_obj_create(wifi_screen);
  lv_obj_set_size(network_container, SCREEN_WIDTH - 30, SCREEN_HEIGHT - 170);
  lv_obj_align(network_container, LV_ALIGN_TOP_LEFT, 15, 140);
  lv_obj_set_style_bg_opa(network_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(network_container, 0, 0);
  lv_obj_set_flex_flow(network_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(network_container, 10, 0);
  
  // Make container scrollable
  lv_obj_add_flag(network_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(network_container, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(network_container, LV_SCROLLBAR_MODE_ACTIVE);
  
  // Set as scrollable object for swipe gestures
  current_scroll_obj = network_container;
  
  // Get saved networks
  auto saved_networks = wifiManager.getSavedNetworks();
  
  // Get scan results (if available)
  auto scan_results = wifiManager.getScanResults();
  
  // Display networks
  std::vector<NetworkInfo> display_networks;
  
  // First add saved networks
  for (const auto& network : saved_networks) {
    bool already_added = false;
    for (const auto& dn : display_networks) {
      if (dn.ssid == network.ssid) {
        already_added = true;
        break;
      }
    }
    
    if (!already_added) {
      display_networks.push_back(network);
    }
  }
  
  // Then add scan results
  for (const auto& network : scan_results) {
    bool already_added = false;
    for (const auto& dn : display_networks) {
      if (dn.ssid == network.ssid) {
        already_added = true;
        break;
      }
    }
    
    if (!already_added) {
      display_networks.push_back(network);
    }
  }
  
  // Sort by signal strength (if scanned) or priority (if saved)
  std::sort(display_networks.begin(), display_networks.end(), 
    [](const NetworkInfo& a, const NetworkInfo& b) {
      if (a.saved && b.saved) {
        return a.priority > b.priority;
      } else if (a.saved) {
        return true;
      } else if (b.saved) {
        return false;
      } else {
        return a.rssi > b.rssi;
      }
    });
  
  // Create network cards
  for (const auto& network : display_networks) {
    // Create network card
    lv_obj_t* card = lv_obj_create(network_container);
    lv_obj_set_size(card, SCREEN_WIDTH - 40, 60);
    lv_obj_add_style(card, &style_card_info, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    // SSID
    lv_obj_t* ssid_label = lv_label_create(card);
    lv_label_set_text(ssid_label, network.ssid.c_str());
    lv_obj_set_style_text_font(ssid_label, &lv_font_montserrat_16, 0);
    lv_obj_align(ssid_label, LV_ALIGN_TOP_LEFT, 10, 5);
    
    // Signal strength or saved indicator
    const char* network_icon;
    if (network.saved) {
      network_icon = LV_SYMBOL_SAVE;
    } else if (network.rssi > -60) {
      network_icon = LV_SYMBOL_WIFI;
    } else if (network.rssi > -75) {
      network_icon = LV_SYMBOL_WIFI;
    } else {
      network_icon = LV_SYMBOL_WIFI;
    }
    
    lv_obj_t* signal_label = lv_label_create(card);
    lv_label_set_text(signal_label, network_icon);
    lv_obj_align(signal_label, LV_ALIGN_TOP_RIGHT, -10, 5);
    
    // Status (connected or security)
    lv_obj_t* status_label = lv_label_create(card);
    if (network.connected) {
      lv_label_set_text(status_label, "Connected");
      lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0);
    } else {
      const char* security;
      switch (network.encryptionType) {
        case WIFI_AUTH_OPEN: security = "Open"; break;
        case WIFI_AUTH_WEP: security = "WEP"; break;
        case WIFI_AUTH_WPA_PSK: security = "WPA"; break;
        case WIFI_AUTH_WPA2_PSK: security = "WPA2"; break;
        case WIFI_AUTH_WPA_WPA2_PSK: security = "WPA/WPA2"; break;
        case WIFI_AUTH_WPA2_ENTERPRISE: security = "Enterprise"; break;
        default: security = "Secured"; break;
      }
      lv_label_set_text(status_label, security);
    }
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_LEFT, 10, -5);
    
    // Add click handler to connect
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    NetworkInfo network_copy = network; // Create a copy for the lambda
    lv_obj_add_event_cb(card, [](lv_event_t* e) {
      NetworkInfo* net_info = (NetworkInfo*)lv_event_get_user_data(e);
      if (net_info) {
        if (net_info->saved) {
          // Try to connect to saved network
          wifiManager.connect(net_info->ssid, net_info->password, false);
        } else {
          // Create a keyboard to enter password
          lv_obj_t* kb_screen = lv_obj_create(NULL);
          lv_obj_add_style(kb_screen, &style_screen, 0);
          
          // Header
          lv_obj_t* kb_header = lv_obj_create(kb_screen);
          lv_obj_set_size(kb_header, SCREEN_WIDTH, 40);
          lv_obj_set_style_bg_color(kb_header, lv_color_hex(0x2D2D2D), 0);
          lv_obj_set_style_bg_opa(kb_header, LV_OPA_COVER, 0);
          lv_obj_clear_flag(kb_header, LV_OBJ_FLAG_SCROLLABLE);
          
          // Title
          lv_obj_t* kb_title = lv_label_create(kb_header);
          lv_label_set_text(kb_title, ("Connect to " + net_info->ssid).c_str());
          lv_obj_set_style_text_font(kb_title, &lv_font_montserrat_16, 0);
          lv_obj_align(kb_title, LV_ALIGN_CENTER, 0, 0);
          
          // Password field
          lv_obj_t* pwd_field = lv_textarea_create(kb_screen);
          lv_textarea_set_text(pwd_field, "");
          lv_textarea_set_password_mode(pwd_field, true);
          lv_textarea_set_placeholder_text(pwd_field, "Enter password");
          lv_obj_set_size(pwd_field, SCREEN_WIDTH - 40, 40);
          lv_obj_align(pwd_field, LV_ALIGN_TOP_MID, 0, 60);
          
          // Show password checkbox
          lv_obj_t* show_pwd_cb = lv_checkbox_create(kb_screen);
          lv_checkbox_set_text(show_pwd_cb, "Show password");
          lv_obj_align(show_pwd_cb, LV_ALIGN_TOP_LEFT, 20, 110);
          
          lv_obj_add_event_cb(show_pwd_cb, [](lv_event_t* e) {
            lv_obj_t* cb = lv_event_get_target(e);
            lv_obj_t* ta = (lv_obj_t*)lv_event_get_user_data(e);
            lv_textarea_set_password_mode(ta, !lv_obj_has_state(cb, LV_STATE_CHECKED));
          }, LV_EVENT_VALUE_CHANGED, pwd_field);
          
          // Keyboard
          lv_obj_t* kb = lv_keyboard_create(kb_screen);
          lv_keyboard_set_textarea(kb, pwd_field);
          lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
          
          // Add event for keyboard button press
          lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
          
          // Connect button
          lv_obj_t* connect_btn = lv_btn_create(kb_screen);
          lv_obj_set_size(connect_btn, 100, 40);
          lv_obj_align(connect_btn, LV_ALIGN_TOP_RIGHT, -20, 110);
          lv_obj_add_style(connect_btn, &style_btn, 0);
          lv_obj_add_style(connect_btn, &style_btn_pressed, LV_STATE_PRESSED);
          
          lv_obj_t* connect_label = lv_label_create(connect_btn);
          lv_label_set_text(connect_label, "Connect");
          lv_obj_center(connect_label);
          
          lv_obj_add_event_cb(connect_btn, [](lv_event_t* e) {
            lv_obj_t* btn = lv_event_get_target(e);
            NetworkInfo* net_info = (NetworkInfo*)lv_event_get_user_data(e);
            lv_obj_t* kb_screen = lv_obj_get_parent(btn);
            
            // Find textarea in children of kb_screen
            lv_obj_t* textarea = NULL;
            for (uint32_t i = 0; i < lv_obj_get_child_cnt(kb_screen); i++) {
              lv_obj_t* child = lv_obj_get_child(kb_screen, i);
              if (lv_obj_check_type(child, &lv_textarea_class)) {
                textarea = child;
                break;
              }
            }
            
            if (textarea && net_info) {
              const char* password = lv_textarea_get_text(textarea);
              wifiManager.connect(net_info->ssid, password, true);
            }
            
            // Return to wifi screen
            createWiFiManagerScreen();
            
          }, LV_EVENT_CLICKED, net_info);
          
          // Cancel button
          lv_obj_t* cancel_btn = lv_btn_create(kb_screen);
          lv_obj_set_size(cancel_btn, 40, 40);
          lv_obj_align(cancel_btn, LV_ALIGN_TOP_LEFT, 20, 60);
          lv_obj_add_style(cancel_btn, &style_btn, 0);
          lv_obj_add_style(cancel_btn, &style_btn_pressed, LV_STATE_PRESSED);
          
          lv_obj_t* cancel_label = lv_label_create(cancel_btn);
          lv_label_set_text(cancel_label, LV_SYMBOL_CLOSE);
          lv_obj_center(cancel_label);
          
          lv_obj_add_event_cb(cancel_btn, [](lv_event_t* e) {
            createWiFiManagerScreen();
          }, LV_EVENT_CLICKED, NULL);
          
          lv_scr_load(kb_screen);
        }
      }
    }, LV_EVENT_CLICKED, &network_copy);
  }
  
  // Add scan button
  lv_obj_t* scan_btn = lv_btn_create(wifi_screen);
  lv_obj_set_size(scan_btn, 100, 40);
  lv_obj_align(scan_btn, LV_ALIGN_BOTTOM_RIGHT, -15, -15);
  lv_obj_add_style(scan_btn, &style_btn, 0);
  lv_obj_add_style(scan_btn, &style_btn_pressed, LV_STATE_PRESSED);
  
  lv_obj_t* scan_label = lv_label_create(scan_btn);
  lv_label_set_text(scan_label, "Scan");
  lv_obj_center(scan_label);
  
  lv_obj_add_event_cb(scan_btn, [](lv_event_t* e) {
    wifiManager.startScan();
    // Refresh screen after scan is complete
    // This is handled in the scan callback in setup()
  }, LV_EVENT_CLICKED, NULL);
  
  // Update the WiFi scan callback to refresh the screen when scan is complete
  wifiManager.setScanCallback([](const std::vector<NetworkInfo>& results) {
    DEBUG_PRINTF("WiFi scan complete, found %d networks", results.size());
    createWiFiManagerScreen();
  });
  
  // Add status bar
  addStatusBar(wifi_screen);
  
  // Load the screen
  lv_scr_load(wifi_screen);
}

// Static function to save time to RTC
static void save_time_to_rtc() {
  DEBUG_PRINTF("Saving time: %04d-%02d-%02d %02d:%02d:00",
               selected_date.year, selected_date.month, selected_date.day,
               selected_hour, selected_minute);

  // Construct the date and time structs
  m5::rtc_date_t DateStruct;
  DateStruct.year = selected_date.year;
  DateStruct.month = selected_date.month;
  DateStruct.date = selected_date.day;
  
  // Simple weekday calculation (not perfect but better than 0)
  // Using Zeller's Congruence algorithm
  int q = selected_date.day;
  int m = selected_date.month;
  int y = selected_date.year;
  if (m < 3) {
    m += 12;
    y--;
  }
  int h = (q + (13 * (m + 1)) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
  DateStruct.weekDay = (h + 1) % 7; // Convert to 0-6 range where 0 is Sunday
  
  M5.Rtc.setDate(&DateStruct);

  m5::rtc_time_t TimeStruct;
  TimeStruct.hours = selected_hour;
  TimeStruct.minutes = selected_minute;
  TimeStruct.seconds = 0;
  M5.Rtc.setTime(&TimeStruct);

  setSystemTimeFromRTC();
  DEBUG_PRINT("Time saved to RTC successfully");
}

// Define the callback functions for date selection
static void on_year_change(lv_event_t* e) {
  // Update the selected date when year changes
  int year = 2020 + lv_roller_get_selected(g_year_roller);
  int month = lv_roller_get_selected(g_month_roller) + 1;
  int day = lv_roller_get_selected(g_day_roller) + 1;
  
  selected_date.year = year;
  selected_date.month = month;
  selected_date.day = day;
  
  char date_str[64];
  snprintf(date_str, sizeof(date_str), "Selected: %04d-%02d-%02d", year, month, day);
  lv_label_set_text(g_selected_date_label, date_str);
}

static void on_month_change(lv_event_t* e) {
  // Update the selected date when month changes
  int year = 2020 + lv_roller_get_selected(g_year_roller);
  int month = lv_roller_get_selected(g_month_roller) + 1;
  int day = lv_roller_get_selected(g_day_roller) + 1;
  
  selected_date.year = year;
  selected_date.month = month;
  selected_date.day = day;
  
  char date_str[64];
  snprintf(date_str, sizeof(date_str), "Selected: %04d-%02d-%02d", year, month, day);
  lv_label_set_text(g_selected_date_label, date_str);
}

static void on_day_change(lv_event_t* e) {
  // Update the selected date when day changes
  int year = 2020 + lv_roller_get_selected(g_year_roller);
  int month = lv_roller_get_selected(g_month_roller) + 1;
  int day = lv_roller_get_selected(g_day_roller) + 1;
  
  selected_date.year = year;
  selected_date.month = month;
  selected_date.day = day;
  
  char date_str[64];
  snprintf(date_str, sizeof(date_str), "Selected: %04d-%02d-%02d", year, month, day);
  lv_label_set_text(g_selected_date_label, date_str);
}

// Define the callback functions for time selection
static void on_hour_change(lv_event_t* e) {
  // Update selected hour when hour roller changes
  selected_hour = lv_roller_get_selected(g_hour_roller);
  
  // Update the displayed time
  char selected_time_str[32];
  snprintf(selected_time_str, sizeof(selected_time_str), "Selected: %02d:%02d", 
           selected_hour, selected_minute);
  lv_label_set_text(g_selected_time_label, selected_time_str);
}

static void on_minute_change(lv_event_t* e) {
  // Update selected minute when minute roller changes
  selected_minute = lv_roller_get_selected(g_minute_roller);
  
  // Update the displayed time
  char selected_time_str[32];
  snprintf(selected_time_str, sizeof(selected_time_str), "Selected: %02d:%02d", 
           selected_hour, selected_minute);
  lv_label_set_text(g_selected_time_label, selected_time_str);
}