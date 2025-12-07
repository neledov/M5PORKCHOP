// Warhog Mode - Wardriving with GPS
#pragma once

#include <Arduino.h>
#include <vector>
#include <map>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../gps/gps.h"
#include "../ml/features.h"

struct WardrivingEntry {
    uint8_t bssid[6];
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    wifi_auth_mode_t authmode;
    double latitude;
    double longitude;
    double altitude;
    uint32_t timestamp;
    bool saved;
    WiFiFeatures features;  // ML features for training data
    uint8_t label;          // 0=unknown, 1=normal, 2=rogue, 3=evil_twin
};

// BSSID key for map lookup (6 bytes as uint64_t)
inline uint64_t bssidToKey(const uint8_t* bssid) {
    return ((uint64_t)bssid[0] << 40) | ((uint64_t)bssid[1] << 32) |
           ((uint64_t)bssid[2] << 24) | ((uint64_t)bssid[3] << 16) |
           ((uint64_t)bssid[4] << 8) | bssid[5];
}

class WarhogMode {
public:
    static void init();
    static void start();
    static void stop();
    static void update();
    static bool isRunning() { return running; }
    
    // Scan control
    static void triggerScan();
    static bool isScanComplete();
    
    // Data access
    static const std::vector<WardrivingEntry>& getEntries() { return entries; }
    static size_t getEntryCount() { return entries.size(); }
    static size_t getNewCount() { return newCount; }
    
    // Export
    static bool exportCSV(const char* path);
    static bool exportKismet(const char* path);
    static bool exportWigle(const char* path);
    static bool exportMLTraining(const char* path);  // ML feature vectors for training
    
    // GPS
    static bool hasGPSFix();
    static GPSData getGPSData();
    
    // Statistics
    static uint32_t getTotalNetworks() { return totalNetworks; }
    static uint32_t getOpenNetworks() { return openNetworks; }
    static uint32_t getWEPNetworks() { return wepNetworks; }
    static uint32_t getWPANetworks() { return wpaNetworks; }
    static uint32_t getSavedCount() { return savedCount; }  // Records written with GPS fix
    
private:
    static bool running;
    static uint32_t lastScanTime;
    static uint32_t scanInterval;
    static bool scanInProgress;
    static uint32_t scanStartTime;
    
    static std::vector<WardrivingEntry> entries;
    static size_t newCount;
    
    // Statistics
    static uint32_t totalNetworks;
    static uint32_t openNetworks;
    static uint32_t wepNetworks;
    static uint32_t wpaNetworks;
    static uint32_t savedCount;     // Records saved with GPS fix
    static String currentFilename;  // Current session CSV file
    
    // Enhanced ML mode - beacon capture
    static bool enhancedMode;
    static std::map<uint64_t, WiFiFeatures> beaconFeatures;  // BSSID -> features from beacons
    static uint32_t beaconCount;
    static volatile bool beaconMapBusy;  // Guard for beacon map access
    
    // Background scan task
    static TaskHandle_t scanTaskHandle;
    static volatile int scanResult;
    
    // Periodic ML export
    static uint32_t lastMLExport;
    static const uint32_t ML_EXPORT_INTERVAL = 60000;  // 60 seconds
    
    static void performScan();
    static void scanTask(void* pvParameters);
    static void processScanResults();
    static void saveNewEntries();  // Auto-save entries with GPS to CSV
    static int findEntry(const uint8_t* bssid);
    static String authModeToString(wifi_auth_mode_t mode);
    static String generateFilename(const char* ext);
    
    // Enhanced mode promiscuous callback
    static void promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type);
    static void startEnhancedCapture();
    static void stopEnhancedCapture();

};
