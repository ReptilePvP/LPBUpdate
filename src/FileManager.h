#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include "Config.h"

// Structure to hold parsed log entry with timestamp
struct LogEntry {
    String text;
    time_t timestamp;
};

class FileManager {
public:
    FileManager(SPIClass &spiClass);
    
    // Initialize SD card
    bool begin();
    
    // File operations
    bool appendToLog(const String& entry);
    bool readLogEntries(std::vector<LogEntry>& entries);
    bool clearLog();
    
    // Check if SD card is available
    bool isReady() const { return ready; }
    
    // Release SPI bus after operations
    void releaseBus();
    
    // Parse timestamp from log entry
    static time_t parseTimestamp(const String& entry);
    
private:
    SPIClass &spi;
    bool ready;
    
    // Private methods
    void acquireBus();
};

#endif // FILE_MANAGER_H