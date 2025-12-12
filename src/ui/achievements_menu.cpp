// Achievements Menu - View unlocked achievements

#include "achievements_menu.h"
#include <M5Cardputer.h>
#include "display.h"
#include "../core/xp.h"

// Static member initialization
uint8_t AchievementsMenu::selectedIndex = 0;
uint8_t AchievementsMenu::scrollOffset = 0;
bool AchievementsMenu::active = false;
bool AchievementsMenu::keyWasPressed = false;
bool AchievementsMenu::showingDetail = false;

// Achievement info - order must match PorkAchievement enum bit positions
static const struct {
    PorkAchievement flag;
    const char* name;
    const char* howTo;
} ACHIEVEMENTS[] = {
    // Original 17 achievements
    { ACH_FIRST_BLOOD,    "F1RST BL00D",    "Capture your first handshake" },
    { ACH_CENTURION,      "C3NTUR10N",      "Find 100 networks in one session" },
    { ACH_MARATHON_PIG,   "MAR4TH0N P1G",   "Walk 10km in a single session" },
    { ACH_NIGHT_OWL,      "N1GHT 0WL",      "Hunt after midnight" },
    { ACH_GHOST_HUNTER,   "GH0ST HUNT3R",   "Find 10 hidden networks" },
    { ACH_APPLE_FARMER,   "4PPLE FARM3R",   "Send 100 Apple BLE packets" },
    { ACH_WARDRIVER,      "WARDR1V3R",      "Log 1000 networks lifetime" },
    { ACH_DEAUTH_KING,    "D3AUTH K1NG",    "Land 100 successful deauths" },
    { ACH_PMKID_HUNTER,   "PMK1D HUNT3R",   "Capture a PMKID" },
    { ACH_WPA3_SPOTTER,   "WPA3 SP0TT3R",   "Find a WPA3 network" },
    { ACH_GPS_MASTER,     "GPS MAST3R",     "Log 100 GPS-tagged networks" },
    { ACH_TOUCH_GRASS,    "T0UCH GR4SS",    "Walk 50km total lifetime" },
    { ACH_SILICON_PSYCHO, "S1L1C0N PSYCH0", "Log 5000 networks lifetime" },
    { ACH_CLUTCH_CAPTURE, "CLUTCH C4PTUR3", "Handshake at <10% battery" },
    { ACH_SPEED_RUN,      "SP33D RUN",      "50 networks in 10 minutes" },
    { ACH_CHAOS_AGENT,    "CH40S AG3NT",    "Send 1000 BLE packets" },
    { ACH_NIETZSWINE,     "N13TZSCH3",      "Stare into the ether long enough" },
    // New 30 achievements
    { ACH_TEN_THOUSAND,   "T3N THOU$AND",   "Log 10,000 networks lifetime" },
    { ACH_NEWB_SNIFFER,   "N3WB SNIFFER",   "Find your first 10 networks" },
    { ACH_FIVE_HUNDRED,   "500 P1GS",       "Find 500 networks in one session" },
    { ACH_OPEN_SEASON,    "OPEN S3ASON",    "Find 50 open networks" },
    { ACH_WEP_LOLZER,     "WEP L0LZER",     "Find a WEP network (ancient relic)" },
    { ACH_HANDSHAKE_HAM,  "HANDSHAK3 HAM",  "Capture 10 handshakes lifetime" },
    { ACH_FIFTY_SHAKES,   "F1FTY SHAKES",   "Capture 50 handshakes lifetime" },
    { ACH_PMKID_FIEND,    "PMK1D F1END",    "Capture 10 PMKIDs" },
    { ACH_TRIPLE_THREAT,  "TR1PLE THREAT",  "Capture 3 handshakes in one session" },
    { ACH_HOT_STREAK,     "H0T STREAK",     "Capture 5 handshakes in one session" },
    { ACH_FIRST_DEAUTH,   "F1RST D3AUTH",   "Your first successful deauth" },
    { ACH_DEAUTH_THOUSAND,"DEAUTH TH0USAND","Land 1000 successful deauths" },
    { ACH_RAMPAGE,        "R4MPAGE",        "10 deauths in one session" },
    { ACH_HALF_MARATHON,  "HALF MARAT0N",   "Walk 21km in a single session" },
    { ACH_HUNDRED_KM,     "HUNDRED K1L0",   "Walk 100km total lifetime" },
    { ACH_GPS_ADDICT,     "GPS 4DD1CT",     "Log 500 GPS-tagged networks" },
    { ACH_ULTRAMARATHON,  "ULTRAMAR4THON",  "Walk 50km in a single session" },
    { ACH_PARANOID_ANDROID,"PARANOID ANDR01D","Send 100 Android FastPair spam" },
    { ACH_SAMSUNG_SPRAY,  "SAMSUNG SPR4Y",  "Send 100 Samsung BLE spam" },
    { ACH_WINDOWS_PANIC,  "W1ND0WS PANIC",  "Send 100 Windows SwiftPair spam" },
    { ACH_BLE_BOMBER,     "BLE B0MBER",     "Send 5000 BLE packets" },
    { ACH_OINKAGEDDON,    "OINK4GEDDON",    "Send 10000 BLE packets" },
    { ACH_SESSION_VET,    "SESS10N V3T",    "Complete 100 sessions" },
    { ACH_FOUR_HOUR_GRIND,"4 HOUR GR1ND",   "4 hour continuous session" },
    { ACH_EARLY_BIRD,     "EARLY B1RD",     "Hunt between 5-7am" },
    { ACH_WEEKEND_WARRIOR,"W33KEND WARR10R","Hunt on a weekend" },
    { ACH_ROGUE_SPOTTER,  "R0GUE SP0TTER",  "ML detects a rogue AP" },
    { ACH_HIDDEN_MASTER,  "H1DDEN MAST3R",  "Find 50 hidden networks" },
    { ACH_WPA3_HUNTER,    "WPA3 HUNT3R",    "Find 25 WPA3 networks" },
    { ACH_MAX_LEVEL,      "MAX L3VEL",      "Reach level 40" },
    { ACH_ABOUT_JUNKIE,   "AB0UT_JUNK13",   "Read the fine print" },
};

void AchievementsMenu::init() {
    selectedIndex = 0;
    scrollOffset = 0;
    showingDetail = false;
}

void AchievementsMenu::show() {
    active = true;
    selectedIndex = 0;
    scrollOffset = 0;
    showingDetail = false;
    keyWasPressed = true;  // Ignore the Enter that selected us from menu
    updateBottomOverlay();
}

void AchievementsMenu::hide() {
    active = false;
    showingDetail = false;
    Display::clearBottomOverlay();
}

void AchievementsMenu::update() {
    if (!active) return;
    handleInput();
}

void AchievementsMenu::handleInput() {
    bool anyPressed = M5Cardputer.Keyboard.isPressed();
    
    if (!anyPressed) {
        keyWasPressed = false;
        return;
    }
    
    if (keyWasPressed) return;
    keyWasPressed = true;
    
    auto keys = M5Cardputer.Keyboard.keysState();
    
    // If showing detail, any key closes it
    if (showingDetail) {
        showingDetail = false;
        return;
    }
    
    // Navigation with ; (up) and . (down)
    if (M5Cardputer.Keyboard.isKeyPressed(';')) {
        if (selectedIndex > 0) {
            selectedIndex--;
            if (selectedIndex < scrollOffset) {
                scrollOffset = selectedIndex;
            }
            updateBottomOverlay();
        }
    }
    
    if (M5Cardputer.Keyboard.isKeyPressed('.')) {
        if (selectedIndex < TOTAL_ACHIEVEMENTS - 1) {
            selectedIndex++;
            if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) {
                scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;
            }
            updateBottomOverlay();
        }
    }
    
    // Enter shows detail for selected achievement
    if (keys.enter) {
        showingDetail = true;
        return;
    }
    
    // Exit with backtick
    if (M5Cardputer.Keyboard.isKeyPressed('`')) {
        hide();
    }
}

void AchievementsMenu::draw(M5Canvas& canvas) {
    if (!active) return;
    
    // If showing detail popup, draw that instead
    if (showingDetail) {
        drawDetail(canvas);
        return;
    }
    
    canvas.fillScreen(TFT_BLACK);
    
    // Get unlocked achievements
    uint64_t unlocked = XP::getAchievements();
    
    canvas.setTextColor(COLOR_FG);
    canvas.setTextSize(1);
    
    // Draw achievements list
    int y = 2;
    int lineHeight = 18;
    
    for (uint8_t i = scrollOffset; i < TOTAL_ACHIEVEMENTS && i < scrollOffset + VISIBLE_ITEMS; i++) {
        bool hasIt = (unlocked & ACHIEVEMENTS[i].flag) != 0;
        
        // Highlight selected (pink bg, black text) - toast style
        if (i == selectedIndex) {
            canvas.fillRect(0, y - 1, canvas.width(), lineHeight, COLOR_FG);
            canvas.setTextColor(COLOR_BG);
        } else {
            canvas.setTextColor(COLOR_FG);
        }
        
        // Lock/unlock indicator
        canvas.setCursor(4, y);
        canvas.print(hasIt ? "[X]" : "[ ]");
        
        // Achievement name (show ??? if locked)
        canvas.setCursor(28, y);
        canvas.print(hasIt ? ACHIEVEMENTS[i].name : "???");
        
        y += lineHeight;
    }
    
    // Scroll indicators
    if (scrollOffset > 0) {
        canvas.setCursor(canvas.width() - 10, 16);
        canvas.setTextColor(COLOR_FG);
        canvas.print("^");
    }
    if (scrollOffset + VISIBLE_ITEMS < TOTAL_ACHIEVEMENTS) {
        canvas.setCursor(canvas.width() - 10, 16 + (VISIBLE_ITEMS - 1) * lineHeight);
        canvas.setTextColor(COLOR_FG);
        canvas.print("v");
    }
}

void AchievementsMenu::drawDetail(M5Canvas& canvas) {
    canvas.fillScreen(COLOR_BG);
    
    bool hasIt = (XP::getAchievements() & ACHIEVEMENTS[selectedIndex].flag) != 0;
    
    // Toast style: pink filled box with black text
    int boxW = 200;
    int boxH = 70;
    int boxX = (canvas.width() - boxW) / 2;
    int boxY = (canvas.height() - boxH) / 2;
    
    canvas.fillRoundRect(boxX, boxY, boxW, boxH, 8, COLOR_FG);
    
    // Black text on pink background
    canvas.setTextColor(COLOR_BG, COLOR_FG);
    canvas.setTextSize(1);
    canvas.setTextDatum(top_center);
    
    // Achievement name (show UNKNOWN if locked)
    canvas.drawString(hasIt ? ACHIEVEMENTS[selectedIndex].name : "UNKNOWN", canvas.width() / 2, boxY + 10);
    
    // Status
    canvas.drawString(hasIt ? "UNLOCKED" : "LOCKED", canvas.width() / 2, boxY + 26);
    
    // How to get it (show ??? if locked)
    canvas.drawString(hasIt ? ACHIEVEMENTS[selectedIndex].howTo : "???", canvas.width() / 2, boxY + 46);
    
    // Reset text datum
    canvas.setTextDatum(top_left);
}

void AchievementsMenu::updateBottomOverlay() {
    uint64_t unlocked = XP::getAchievements();
    bool hasIt = (unlocked & ACHIEVEMENTS[selectedIndex].flag) != 0;
    
    if (hasIt) {
        Display::setBottomOverlay(ACHIEVEMENTS[selectedIndex].howTo);
    } else {
        Display::setBottomOverlay("UNKNOWN");
    }
}
