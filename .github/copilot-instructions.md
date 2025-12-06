# M5Porkchop Copilot Instructions

## Project Overview

M5Porkchop is a WiFi security research tool for the M5Cardputer (ESP32-S3 based) with a "piglet" mascot personality. It features:
- **OINK Mode**: WiFi scanning, handshake capture, deauth attacks
- **WARHOG Mode**: Wardriving with GPS logging
- Interactive ASCII piglet avatar with mood-based phrases
- Settings menu with persistent configuration

## Architecture

### Core Components
- `src/core/porkchop.cpp/h` - Main state machine, mode management, event system
- `src/core/config.cpp/h` - Configuration structs (GPSConfig, WiFiConfig, PersonalityConfig), load/save to SPIFFS

### Modes
- `src/modes/oink.cpp/h` - OinkMode: WiFi scanning, channel hopping, promiscuous mode, handshake capture
- `src/modes/warhog.cpp/h` - WarhogMode: GPS-enabled wardriving, CSV export

### UI Layer
- `src/ui/display.cpp/h` - Triple-buffered canvas system (topBar, mainCanvas, bottomBar), 240x135 display
- `src/ui/menu.cpp/h` - Main menu with callback system
- `src/ui/settings_menu.cpp/h` - Interactive settings with TOGGLE, VALUE, ACTION item types

### Piglet Personality
- `src/piglet/avatar.cpp/h` - ASCII art pig face rendering, AvatarState (NEUTRAL, HUNTING, EXCITED, SLEEPING, HAPPY, SAD)
- `src/piglet/mood.cpp/h` - Context-aware phrases, happiness tracking, mode-specific phrase arrays

### Hardware
- `src/gps/gps.cpp/h` - TinyGPS++ wrapper, power management
- `src/ml/` - ML inference stubs for future rogue AP detection

## Display System

```
┌────────────────────────────────────────┐
│ TOP_BAR (14px) - Mode indicator, icons │
├────────────────────────────────────────┤
│                                        │
│ MAIN_CANVAS (107px)                    │
│ - Avatar on left (ASCII pig)           │
│ - Speech bubble on right               │
│                                        │
├────────────────────────────────────────┤
│ BOTTOM_BAR (14px) - Stats: N:x HS:x D:x│
└────────────────────────────────────────┘
```

- **Colors**: COLOR_FG = 0xFD75 (piglet pink), COLOR_BG = 0x0000 (black)
- **Display**: 240x135 pixels, landscape orientation

## Keyboard Navigation (M5Cardputer)

- **`;`** - Previous/Up/Decrease value
- **`.`** - Next/Down/Increase value
- **Enter** - Select/Toggle/Confirm
- **Backtick (`)** - Open menu / Exit to previous mode
- **O/W/S** - Quick mode shortcuts from IDLE (Oink/Warhog/Settings)

## Mode State Machine

```
PorkchopMode:
  IDLE -> OINK_MODE | WARHOG_MODE | MENU | SETTINGS | ABOUT
  MENU -> (any mode via menu selection)
  SETTINGS/ABOUT -> MENU (via Enter or backtick)
```

**Important**: `previousMode` only stores "real" modes (IDLE, OINK_MODE, WARHOG_MODE), never MENU/SETTINGS/ABOUT, to prevent navigation loops.

## Phrase System

Mood phrases are context-aware arrays in `mood.cpp`:
- `PHRASES_HAPPY[]` - On handshake capture
- `PHRASES_HUNTING[]` - During OINK mode scanning
- `PHRASES_WARHOG[]` - During WARHOG mode
- `PHRASES_WARHOG_FOUND[]` - When new AP with GPS is logged
- `PHRASES_SAD[]` - No activity
- `PHRASES_IDLE[]` / `PHRASES_MENU_IDLE[]` - Menu/idle state

Call appropriate `Mood::onXxx()` method to trigger phrase updates.

## Settings Menu

Settings use `SettingItem` struct with types:
- `TOGGLE` - ON/OFF, Enter toggles, arrows navigate
- `VALUE` - Numeric with min/max/step, arrows adjust directly
- `ACTION` - Triggers function (e.g., Save & Exit)

Settings persist to `/porkchop.conf` via ArduinoJson.

## Build Commands

```powershell
pio run                        # Build all
pio run -e m5cardputer         # Build release only
pio run -t upload              # Build and upload
pio run -t upload -e m5cardputer  # Upload release only
```

## Code Style Guidelines

1. **Static classes** for singletons (OinkMode, WarhogMode, Menu, Display, etc.)
2. **Include guards** with `#pragma once`
3. **Relative includes** from src root: `#include "../core/config.h"`
4. **Debug logging** via `Serial.printf("[MODULE] message\n")`
5. **Debounce pattern** for keyboard: track `keyWasPressed` bool, only act on edge transitions

## Common Patterns

### Adding a new phrase category
```cpp
// In mood.cpp
const char* PHRASES_NEWMODE[] = {
    "Phrase 1",
    "Phrase 2"
};

void Mood::onNewModeEvent() {
    int idx = random(0, sizeof(PHRASES_NEWMODE) / sizeof(PHRASES_NEWMODE[0]));
    currentPhrase = PHRASES_NEWMODE[idx];
    lastPhraseChange = millis();
}
```

### Adding a new setting
```cpp
// In settings_menu.cpp loadFromConfig()
items.push_back({
    "Label",
    SettingType::VALUE,  // or TOGGLE
    initialValue,
    minVal, maxVal, step, "suffix"
});

// In saveToConfig() - apply the value
```

### Handling new mode
```cpp
// In porkchop.cpp setMode()
case PorkchopMode::NEW_MODE:
    NewMode::start();
    Avatar::setState(AvatarState::STATE);
    break;
```

## Hardware Specifics

- **M5Cardputer**: ESP32-S3, 240x135 ST7789 display, TCA8418 keyboard controller
- **GPS**: Connected via UART (pins configurable in GPSConfig)
- **SD Card**: For handshake/wardriving data export
- **WiFi**: ESP32 native, promiscuous mode for packet capture

## Testing

No automated tests currently. Test on hardware:
1. Verify mode transitions
2. Check phrase display in each mode
3. Confirm settings save/load across reboots
4. Test GPS lock and wardriving CSV export
