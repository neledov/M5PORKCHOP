// HOG ON SPECTRUM Mode - WiFi Spectrum Analyzer
#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <vector>
#include <esp_wifi_types.h>

struct SpectrumNetwork {
    uint8_t bssid[6];
    char ssid[33];
    uint8_t channel;         // 1-13
    int8_t rssi;             // Latest RSSI
    uint32_t lastSeen;       // millis() of last beacon
    wifi_auth_mode_t authmode; // Security type (OPEN/WEP/WPA/WPA2/WPA3)
    bool hasPMF;             // Protected Management Frames (immune to deauth)
};

class SpectrumMode {
public:
    static void init();
    static void start();
    static void stop();
    static void update();
    static void draw(M5Canvas& canvas);
    static bool isRunning() { return running; }
    
    // For promiscuous callback - updates network RSSI
    static void onBeacon(const uint8_t* bssid, uint8_t channel, int8_t rssi, const char* ssid, wifi_auth_mode_t authmode, bool hasPMF);
    
    // Bottom bar info
    static String getSelectedInfo();
    
private:
    static bool running;
    static volatile bool busy;       // Guard against callback race
    static std::vector<SpectrumNetwork> networks;
    static float viewCenterMHz;      // Center of visible spectrum
    static float viewWidthMHz;       // Visible bandwidth
    static int selectedIndex;        // Currently highlighted network
    static uint32_t lastUpdateTime;
    static bool keyWasPressed;
    static uint8_t currentChannel;   // Current hop channel
    static uint32_t lastHopTime;     // Last channel hop time
    static uint32_t startTime;       // When mode started (for achievement)
    
    static void handleInput();
    static void drawSpectrum(M5Canvas& canvas);
    static void drawGaussianLobe(M5Canvas& canvas, float centerFreqMHz, int8_t rssi, bool filled);
    static void drawAxis(M5Canvas& canvas);
    static void drawChannelMarkers(M5Canvas& canvas);
    static void pruneStale();        // Remove networks not seen recently
    
    // Coordinate mapping
    static int freqToX(float freqMHz);
    static int rssiToY(int8_t rssi);
    static float channelToFreq(uint8_t channel);
    
    // Security helpers
    static bool isVulnerable(wifi_auth_mode_t mode);
    static const char* authModeToShortString(wifi_auth_mode_t mode);
    static bool detectPMF(const uint8_t* payload, uint16_t len);
    
    // Promiscuous mode
    static void promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type);
};
