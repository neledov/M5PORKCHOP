// Display management for M5Cardputer
#pragma once

#include <M5Unified.h>

// Forward declarations
enum class PorkchopMode : uint8_t;

// Display layout constants (240x135 screen)
#define DISPLAY_W 240
#define DISPLAY_H 135
#define TOP_BAR_H 14
#define BOTTOM_BAR_H 14
#define MAIN_H (DISPLAY_H - TOP_BAR_H - BOTTOM_BAR_H)

// Theme structure
struct PorkTheme {
    const char* name;
    uint16_t fg;
    uint16_t bg;
};

// Available themes (10 total: 6 dark + 4 inverted)
static const PorkTheme THEMES[] = {
    // Dark modes (colored text on black)
    {"P1NK",      0xFD75, 0x0000},  // Default piglet pink
    {"CYB3R",     0x07FF, 0x0000},  // Cyan/tron
    {"M4TR1X",    0x07E0, 0x0000},  // Green
    {"AMB3R",     0xFD20, 0x0000},  // Amber terminal
    {"BL00D",     0xF800, 0x0000},  // Red
    {"GH0ST",     0xFFFF, 0x0000},  // White mono
    // Inverted modes (black text on colored bg)
    {"PAP3R",     0x0000, 0xFFFF},  // Black on white
    {"BUBBLEGUM", 0x0000, 0xFD75},  // Black on pink
    {"M1NT",      0x0000, 0x07FF},  // Black on cyan
    {"SUNBURN",   0x0000, 0xFD20},  // Black on amber
};
static const uint8_t THEME_COUNT = 10;

// Dynamic color getters (use these instead of macros)
uint16_t getColorFG();
uint16_t getColorBG();

// Compatibility macros - redirect to getters
#define COLOR_BG getColorBG()
#define COLOR_FG getColorFG()
#define COLOR_ACCENT COLOR_FG
#define COLOR_WARNING COLOR_FG
#define COLOR_DANGER COLOR_FG
#define COLOR_SUCCESS COLOR_FG

class Display {
public:
    static void init();
    static void update();
    static void clear();
    
    // Canvas access for direct drawing
    static M5Canvas& getTopBar() { return topBar; }
    static M5Canvas& getMain() { return mainCanvas; }
    static M5Canvas& getBottomBar() { return bottomBar; }
    
    // Helper functions
    static void pushAll();
    static void showBootSplash();  // 3-screen boot animation
    static void showInfoBox(const String& title, const String& line1, 
                           const String& line2 = "", bool blocking = true);
    static bool showConfirmBox(const String& title, const String& message);
    static void showProgress(const String& title, uint8_t percent);
    static void showToast(const String& message);  // Quick non-blocking message
    static void showLevelUp(uint8_t oldLevel, uint8_t newLevel);  // RPG level up popup
    static void showClassPromotion(const char* oldClass, const char* newClass);  // Class tier promotion popup
    
    // PWNED banner (shown in top bar for 1 minute after capture)
    static void showLoot(const String& ssid);
    
    // Bottom bar overlay (for confirmation dialogs)
    static void setBottomOverlay(const String& message);  // Set custom bottom bar text
    static void clearBottomOverlay();                     // Clear overlay, restore normal
    
    // Status indicators
    static void setGPSStatus(bool hasFix);
    static void setWiFiStatus(bool connected);
    static void setMLStatus(bool active);
    
    // Screen dimming
    static void resetDimTimer();      // Call on any user input
    static void updateDimming();      // Call in update loop
    static bool isDimmed() { return dimmed; }
    
    // Screenshot
    static bool takeScreenshot();     // Save screen to SD card, returns success
    static bool isSnapping() { return snapping; }  // True during screenshot save
    
private:
    static M5Canvas topBar;
    static M5Canvas mainCanvas;
    static M5Canvas bottomBar;
    
    static bool gpsStatus;
    static bool wifiStatus;
    static bool mlStatus;
    
    // Dimming state
    static uint32_t lastActivityTime;
    static bool dimmed;
    
    // Screenshot state
    static bool snapping;
    
    // Bottom bar overlay
    static String bottomOverlay;
    
    static void drawTopBar();
    static void drawBottomBar();
    static void drawModeInfo(M5Canvas& canvas, PorkchopMode mode);
    static void drawSettingsScreen(M5Canvas& canvas);
    static void drawAboutScreen(M5Canvas& canvas);
    static void drawFileTransferScreen(M5Canvas& canvas);
    
public:
    // About screen easter egg handlers (called from porkchop.cpp)
    static void onAboutEnterPressed();
    static void resetAboutState();
};
