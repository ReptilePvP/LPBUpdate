#ifndef CONFIG_H
#define CONFIG_H

// Debug settings
#define DEBUG_ENABLED true
#define DEBUG_PRINT(x) if(DEBUG_ENABLED) { Serial.print(millis()); Serial.print(": "); Serial.println(x); }
#define DEBUG_PRINTF(x, ...) if(DEBUG_ENABLED) { Serial.print(millis()); Serial.print(": "); Serial.printf(x, __VA_ARGS__); }

// Hardware pin definitions for M5Stack CoreS3
#define SD_SPI_SCK_PIN  36
#define SD_SPI_MISO_PIN 35
#define SD_SPI_MOSI_PIN 37
#define SD_SPI_CS_PIN   4
#define TFT_DC 35

// Hardware I2C addresses
#define AXP2101_ADDR 0x34
#define AW9523_ADDR 0x58

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// LVGL refresh period
#define LV_TICK_PERIOD_MS 10

// WiFi parameters
#define MAX_NETWORKS 5
#define DEFAULT_SSID "Wack House"
#define DEFAULT_PASS "justice69"
#define WIFI_RECONNECT_INTERVAL 10000  // 10 seconds between connection attempts
#define MAX_WIFI_CONNECTION_ATTEMPTS 5  // Maximum number of consecutive connection attempts

// File system
#define LOG_FILENAME "/loss_prevention_log.txt"

#endif // CONFIG_H