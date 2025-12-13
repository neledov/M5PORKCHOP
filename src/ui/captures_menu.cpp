// Captures Menu - View saved handshake captures

#include "captures_menu.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <time.h>
#include "display.h"

// Static member initialization
std::vector<CaptureInfo> CapturesMenu::captures;
uint8_t CapturesMenu::selectedIndex = 0;
uint8_t CapturesMenu::scrollOffset = 0;
bool CapturesMenu::active = false;
bool CapturesMenu::keyWasPressed = false;
bool CapturesMenu::nukeConfirmActive = false;

void CapturesMenu::init() {
    captures.clear();
    selectedIndex = 0;
    scrollOffset = 0;
}

void CapturesMenu::show() {
    active = true;
    selectedIndex = 0;
    scrollOffset = 0;
    keyWasPressed = true;  // Ignore the Enter that selected us from menu
    scanCaptures();
}

void CapturesMenu::hide() {
    active = false;
}

void CapturesMenu::scanCaptures() {
    captures.clear();
    
    if (!SD.exists("/handshakes")) {
        Serial.println("[CAPTURES] No handshakes directory");
        return;
    }
    
    File dir = SD.open("/handshakes");
    if (!dir || !dir.isDirectory()) {
        Serial.println("[CAPTURES] Failed to open handshakes directory");
        return;
    }
    
    File file = dir.openNextFile();
    while (file) {
        String name = file.name();
        bool isPCAP = name.endsWith(".pcap");
        bool isPMKID = name.endsWith(".22000") && !name.endsWith("_hs.22000");
        bool isHS22000 = name.endsWith("_hs.22000");
        
        // Skip PCAP if we have the corresponding _hs.22000 (avoid duplicates)
        // We prefer showing _hs.22000 because it's hashcat-ready
        if (isPCAP) {
            String baseName = name.substring(0, name.indexOf('.'));
            String hs22kPath = "/handshakes/" + baseName + "_hs.22000";
            if (SD.exists(hs22kPath)) {
                file = dir.openNextFile();
                continue;  // Skip this PCAP, _hs.22000 will be shown instead
            }
        }
        
        if (isPCAP || isPMKID || isHS22000) {
            CaptureInfo info;
            info.filename = name;
            info.fileSize = file.size();
            info.captureTime = file.getLastWrite();
            info.isPMKID = isPMKID;  // Only true for actual PMKID files
            
            // Extract BSSID from filename (e.g., "64EEB7208286.pcap" or "64EEB7208286_hs.22000")
            String baseName = name.substring(0, name.indexOf('.'));
            // Handle _hs suffix for handshake 22000 files
            if (baseName.endsWith("_hs")) {
                baseName = baseName.substring(0, baseName.length() - 3);
            }
            if (baseName.length() >= 12) {
                info.bssid = baseName.substring(0, 2) + ":" +
                             baseName.substring(2, 4) + ":" +
                             baseName.substring(4, 6) + ":" +
                             baseName.substring(6, 8) + ":" +
                             baseName.substring(8, 10) + ":" +
                             baseName.substring(10, 12);
            } else {
                info.bssid = baseName;
            }
            
            // Try to get SSID from companion .txt file if exists
            // PMKID uses _pmkid.txt suffix, handshake uses .txt
            String txtPath = isPMKID ? 
                "/handshakes/" + baseName + "_pmkid.txt" :
                "/handshakes/" + baseName + ".txt";
            if (SD.exists(txtPath)) {
                File txtFile = SD.open(txtPath, FILE_READ);
                if (txtFile) {
                    info.ssid = txtFile.readStringUntil('\n');
                    info.ssid.trim();
                    txtFile.close();
                }
            }
            if (info.ssid.isEmpty()) {
                info.ssid = "[unknown]";
            }
            
            captures.push_back(info);
        }
        file = dir.openNextFile();
    }
    dir.close();
    
    // Sort by capture time (newest first)
    std::sort(captures.begin(), captures.end(), [](const CaptureInfo& a, const CaptureInfo& b) {
        return a.captureTime > b.captureTime;
    });
    
    Serial.printf("[CAPTURES] Found %d captures\n", captures.size());
}

void CapturesMenu::update() {
    if (!active) return;
    handleInput();
}

void CapturesMenu::handleInput() {
    bool anyPressed = M5Cardputer.Keyboard.isPressed();
    
    if (!anyPressed) {
        keyWasPressed = false;
        return;
    }
    
    if (keyWasPressed) return;
    keyWasPressed = true;
    
    auto keys = M5Cardputer.Keyboard.keysState();
    
    // Handle nuke confirmation modal
    if (nukeConfirmActive) {
        if (M5Cardputer.Keyboard.isKeyPressed('y') || M5Cardputer.Keyboard.isKeyPressed('Y')) {
            nukeLoot();
            nukeConfirmActive = false;
            scanCaptures();  // Refresh list (should be empty now)
        } else if (M5Cardputer.Keyboard.isKeyPressed('n') || M5Cardputer.Keyboard.isKeyPressed('N') ||
                   M5Cardputer.Keyboard.isKeyPressed('`') || keys.enter) {
            nukeConfirmActive = false;  // Cancel
        }
        return;
    }
    
    // Navigation with ; (up) and . (down)
    if (M5Cardputer.Keyboard.isKeyPressed(';')) {
        if (selectedIndex > 0) {
            selectedIndex--;
            if (selectedIndex < scrollOffset) {
                scrollOffset = selectedIndex;
            }
        }
    }
    
    if (M5Cardputer.Keyboard.isKeyPressed('.')) {
        if (!captures.empty() && selectedIndex < captures.size() - 1) {
            selectedIndex++;
            if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) {
                scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;
            }
        }
    }
    
    // Nuke all loot with D key
    if (M5Cardputer.Keyboard.isKeyPressed('d') || M5Cardputer.Keyboard.isKeyPressed('D')) {
        if (!captures.empty()) {
            nukeConfirmActive = true;
        }
    }
    
    // Exit with backtick or Enter
    if (keys.enter || M5Cardputer.Keyboard.isKeyPressed('`')) {
        hide();
    }
}

String CapturesMenu::formatTime(time_t t) {
    if (t == 0) return "Unknown";
    
    struct tm* timeinfo = localtime(&t);
    if (!timeinfo) return "Unknown";
    
    char buf[32];
    // Format: "Dec 06 14:32"
    strftime(buf, sizeof(buf), "%b %d %H:%M", timeinfo);
    return String(buf);
}

void CapturesMenu::draw(M5Canvas& canvas) {
    if (!active) return;
    
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(COLOR_FG);
    canvas.setTextSize(1);
    
    if (captures.empty()) {
        canvas.setCursor(4, 40);
        canvas.print("No captures found");
        canvas.setCursor(4, 55);
        canvas.print("[O] to hunt.");
        return;
    }
    
    // Draw captures list
    int y = 2;
    int lineHeight = 18;
    
    for (uint8_t i = scrollOffset; i < captures.size() && i < scrollOffset + VISIBLE_ITEMS; i++) {
        const CaptureInfo& cap = captures[i];
        
        // Highlight selected
        if (i == selectedIndex) {
            canvas.fillRect(0, y - 1, canvas.width(), lineHeight, COLOR_FG);
            canvas.setTextColor(TFT_BLACK);
        } else {
            canvas.setTextColor(COLOR_FG);
        }
        
        // SSID (truncated if needed) - show [P] prefix for PMKID
        canvas.setCursor(4, y);
        String displaySSID = cap.isPMKID ? "[P]" : "";
        displaySSID += cap.ssid;
        if (displaySSID.length() > 14) {
            displaySSID = displaySSID.substring(0, 12) + "..";
        }
        canvas.print(displaySSID);
        
        // Date/time
        canvas.setCursor(95, y);
        canvas.print(formatTime(cap.captureTime));
        
        // File size (KB)
        canvas.setCursor(170, y);
        canvas.printf("%dK", cap.fileSize / 1024);
        
        y += lineHeight;
    }
    
    // Scroll indicators
    if (scrollOffset > 0) {
        canvas.setCursor(canvas.width() - 10, 16);
        canvas.setTextColor(COLOR_FG);
        canvas.print("^");
    }
    if (scrollOffset + VISIBLE_ITEMS < captures.size()) {
        canvas.setCursor(canvas.width() - 10, 16 + (VISIBLE_ITEMS - 1) * lineHeight);
        canvas.setTextColor(COLOR_FG);
        canvas.print("v");
    }
    
    // Draw nuke confirmation modal if active
    if (nukeConfirmActive) {
        drawNukeConfirm(canvas);
    }
    // BSSID shown in bottom bar via getSelectedBSSID()
}

void CapturesMenu::drawNukeConfirm(M5Canvas& canvas) {
    // Modal box dimensions
    const int boxW = 180;
    const int boxH = 60;
    const int boxX = (canvas.width() - boxW) / 2;
    const int boxY = (canvas.height() - boxH) / 2 - 5;
    
    // Draw box with border
    canvas.fillRect(boxX - 2, boxY - 2, boxW + 4, boxH + 4, COLOR_FG);
    canvas.fillRect(boxX, boxY, boxW, boxH, TFT_BLACK);
    
    canvas.setTextColor(COLOR_FG);
    canvas.setTextSize(1);
    
    // Hacker edgy message
    canvas.setCursor(boxX + 8, boxY + 8);
    canvas.print("SCORCHED EARTH MODE");
    canvas.setCursor(boxX + 8, boxY + 22);
    canvas.print("rm -rf /handshakes/*");
    canvas.setCursor(boxX + 8, boxY + 36);
    canvas.print("This kills the loot.");
    canvas.setCursor(boxX + 8, boxY + 50);
    canvas.print("[Y] Do it  [N] Abort");
}

void CapturesMenu::nukeLoot() {
    Serial.println("[CAPTURES] Nuking all loot...");
    
    if (!SD.exists("/handshakes")) {
        return;
    }
    
    File dir = SD.open("/handshakes");
    if (!dir || !dir.isDirectory()) {
        return;
    }
    
    // Collect filenames first (can't delete while iterating)
    std::vector<String> files;
    File file = dir.openNextFile();
    while (file) {
        files.push_back(String("/handshakes/") + file.name());
        file = dir.openNextFile();
    }
    dir.close();
    
    // Delete all files
    int deleted = 0;
    for (const auto& path : files) {
        if (SD.remove(path)) {
            deleted++;
        }
    }
    
    Serial.printf("[CAPTURES] Nuked %d files\n", deleted);
    
    // Reset selection
    selectedIndex = 0;
    scrollOffset = 0;
    captures.clear();
}

String CapturesMenu::getSelectedBSSID() {
    if (selectedIndex < captures.size()) {
        return captures[selectedIndex].bssid;
    }
    return "";
}
