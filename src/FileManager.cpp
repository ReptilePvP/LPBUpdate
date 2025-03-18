#include "FileManager.h"
#include <time.h>

FileManager::FileManager(SPIClass &spiClass) : spi(spiClass), ready(false) {
}

bool FileManager::begin() {
    DEBUG_PRINT("Initializing SD card...");
    
    // Stop the default SPI (used by LCD)
    ::SPI.end();
    delay(200); // Give it a moment to settle
    
    // Initialize SPI for SD card
    spi.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, -1); // -1 means no default CS
    pinMode(SD_SPI_CS_PIN, OUTPUT); // Set CS pin as OUTPUT
    digitalWrite(SD_SPI_CS_PIN, HIGH); // Deselect SD card (HIGH = off)
    delay(200); // Wait for SD card to stabilize
    
    bool sdInitialized = false;
    
    // Try initializing the SD card up to 3 times
    for (int i = 0; i < 3 && !sdInitialized; i++) {
        digitalWrite(SD_SPI_CS_PIN, LOW); // Select SD card
        sdInitialized = SD.begin(SD_SPI_CS_PIN, spi, 25000000); // 25MHz max for SD cards
        digitalWrite(SD_SPI_CS_PIN, HIGH); // Deselect SD card
        
        if (sdInitialized) {
            DEBUG_PRINT("SD card initialized successfully");
            break;
        }
        
        DEBUG_PRINTF("SD card initialization attempt %d failed", i+1);
        delay(200); // Wait before retrying
    }
    
    if (!sdInitialized) {
        DEBUG_PRINT("All SD card initialization attempts failed");
        releaseBus();
        return false;
    }
    
    ready = true;
    
    // Check card type and print info
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        DEBUG_PRINT("No SD card attached");
        ready = false;
    } else {
        DEBUG_PRINT("SD Card Type: ");
        if (cardType == CARD_MMC) {
            DEBUG_PRINT("MMC");
        } else if (cardType == CARD_SD) {
            DEBUG_PRINT("SDSC");
        } else if (cardType == CARD_SDHC) {
            DEBUG_PRINT("SDHC");
        } else {
            DEBUG_PRINT("UNKNOWN");
        }
        
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        DEBUG_PRINTF("SD Card Size: %lluMB\n", cardSize);
    }
    
    // Check if log file exists, create it if not
    if (!SD.exists(LOG_FILENAME)) {
        File file = SD.open(LOG_FILENAME, FILE_WRITE);
        if (file) {
            file.println("# Loss Prevention Log - Created");
            file.close();
            DEBUG_PRINT("Created new log file");
        }
    }
    
    releaseBus();
    return ready;
}

void FileManager::acquireBus() {
    ::SPI.end();
    delay(100);
    spi.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, -1);
    pinMode(SD_SPI_CS_PIN, OUTPUT);
    digitalWrite(SD_SPI_CS_PIN, HIGH);
    delay(100);
}

void FileManager::releaseBus() {
    spi.end();
    ::SPI.begin();
    pinMode(TFT_DC, OUTPUT);
    digitalWrite(TFT_DC, HIGH);
}

bool FileManager::appendToLog(const String& entry) {
    if (!ready) {
        DEBUG_PRINT("SD card not initialized, cannot save entry");
        return false;
    }
    
    acquireBus();
    
    File file = SD.open(LOG_FILENAME, FILE_APPEND);
    if (!file) {
        DEBUG_PRINT("Failed to open log file for writing");
        releaseBus();
        return false;
    }
    
    // Format with timestamp
    String timestamp = "";
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char buffer[25];
        strftime(buffer, sizeof(buffer), "%d-%b-%Y %H:%M:%S", &timeinfo);
        timestamp = buffer;
    }
    
    String formattedEntry = timestamp + ": " + entry;
    DEBUG_PRINTF("Raw entry text: %s\n", formattedEntry.c_str());
    size_t bytesWritten = file.println(formattedEntry);
    file.close();
    
    releaseBus();
    
    if (bytesWritten == 0) {
        DEBUG_PRINT("Failed to write to log file");
        return false;
    }
    
    DEBUG_PRINTF("Entry saved to file (%d bytes): %s\n", bytesWritten, formattedEntry.c_str());
    return true;
}

bool FileManager::readLogEntries(std::vector<LogEntry>& entries) {
    if (!ready) {
        DEBUG_PRINT("SD card not initialized, cannot read entries");
        return false;
    }
    
    acquireBus();
    
    File file = SD.open(LOG_FILENAME, FILE_READ);
    if (!file) {
        DEBUG_PRINT("Failed to open log file for reading");
        releaseBus();
        return false;
    }
    
    entries.clear();
    String entryText = "";
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() > 0 && !line.startsWith("#")) {
            time_t timestamp = parseTimestamp(line);
            if (timestamp != 0) {
                LogEntry entry;
                entry.text = line;
                entry.timestamp = timestamp;
                entries.push_back(entry);
                DEBUG_PRINTF("Parsed entry: %s\n", line.c_str());
            } else {
                DEBUG_PRINTF("Failed to parse timestamp: %s\n", line.c_str());
            }
        }
    }
    
    file.close();
    releaseBus();
    
    DEBUG_PRINTF("Read %d valid log entries from file\n", entries.size());
    return true;
}

bool FileManager::clearLog() {
    if (!ready) {
        DEBUG_PRINT("SD card not initialized, cannot clear log");
        return false;
    }
    
    acquireBus();
    
    // Remove the old file and create a new one
    bool success = SD.remove(LOG_FILENAME);
    
    if (success) {
        // Create a new empty log file
        File file = SD.open(LOG_FILENAME, FILE_WRITE);
        if (file) {
            file.println("# Loss Prevention Log - Created (Reset)");
            file.close();
            DEBUG_PRINT("Log cleared successfully");
        } else {
            DEBUG_PRINT("Failed to create new log file after clear");
            success = false;
        }
    } else {
        DEBUG_PRINT("Failed to remove log file");
    }
    
    releaseBus();
    return success;
}

time_t FileManager::parseTimestamp(const String& entry) {
    struct tm timeinfo = {0};
    int day, year, hour, minute, second;
    char monthStr[4]; // For 3-letter month abbreviation + null terminator

    // Expecting format: "DD-MMM-YYYY HH:MM:SS" (e.g., "05-Mar-2025 08:12:24")
    if (sscanf(entry.c_str(), "%d-%3s-%d %d:%d:%d", &day, monthStr, &year, &hour, &minute, &second) == 6) {
        timeinfo.tm_mday = day;
        timeinfo.tm_year = year - 1900; // Years since 1900
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = minute;
        timeinfo.tm_sec = second;
        timeinfo.tm_isdst = -1; // Let mktime figure out DST

        // Convert 3-letter month abbreviation to month number (0-11)
        static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        for (int i = 0; i < 12; i++) {
            if (strncmp(monthStr, months[i], 3) == 0) {
                timeinfo.tm_mon = i;
                break;
            }
        }

        return mktime(&timeinfo);
    }
    return 0; // Invalid timestamp
}