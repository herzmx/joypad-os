# Web Config Comparison: Joypad OS vs GP2040-CE

Detailed feature gap analysis for prioritizing web config development.

## Pages Overview

| Joypad OS | GP2040-CE |
|-----------|-----------|
| Device Info | Home (version, memory) |
| Router | Settings (input/d-pad/SOCD modes) |
| Profiles | (built into Pin Mapping) |
| Hotkeys | Settings (hotkey actions) |
| Custom Pad | Pin Mapping |
| Bluetooth Host | (no equivalent — limited BT) |
| USB Host | (no equivalent — single USB role) |
| Feedback (LED + Buzzer) | LED Configuration + Buzzer addon |
| Bluetooth Device | (no BLE peripheral) |
| USB Device | (mode is in Settings) |
| Input Test | (no equivalent) |
| Advanced (reset) | Reset Settings + Backup/Restore |
| — | Addons Configuration |
| — | Display Configuration (OLED/LCD) |
| — | Custom Theme |
| — | Input Macros |
| — | Keyboard Mapping |
| — | Peripheral Mapping (I2C) |
| — | Backup/Restore |

## Joypad OS Strengths (we have, they don't)

- **BLE peripheral output** — Bluetooth Device mode (gamepad advertising)
- **BLE central input** — Onboard BT Host scanning
- **USB Host input** — Multi-controller support via PIO-USB or MAX3421E
- **Cross-platform** — RP2040, ESP32-S3, nRF52840 (GP2040-CE is RP2040 only)
- **Per-player Input Test** — Visual input stream visualization with merge mode
- **Dirty tracking** — Save buttons disabled until changes (UX polish)
- **Runtime config without reboot** — D-pad mode, Wiimote orientation
- **Web Bluetooth (BLE NUS)** — Wireless web config option
- **Firmware update check + OTA** — Built into web config
- **Modern dark theme + sidebar nav** — Cleaner UI architecture

## GP2040-CE Strengths (gaps to fill)

### Critical (high user impact)

1. **Add-on system** — Modular architecture for ~20 features. Without this, every new feature is hardcoded. Implementing the add-on framework would unlock:
   - Turbo (rapid-fire with shot count)
   - Analog (stick inversion, circular deadzone)
   - SOCD slider (5 modes, hardware switching)
   - Tilt sensor input
   - Reverse buttons
   - Buzzer/PlayerNumber LEDs
   - Bootsel button
   - Focus Mode (arcade)
   - PS4 authentication
   - PS3/PS4/Xbox One controller passthrough

2. **Display configuration** — OLED/LCD support entirely missing in our web config:
   - I2C address, display size
   - Splash image bitmap editor
   - Multiple button layouts (8-Button, HitBox, WASD, etc.)
   - Flip/mirror/invert
   - Per-mode display content

3. **LED themes & effects** — We only have enable/pin/count. They have:
   - Per-button color picker
   - Pre-built themes
   - Gradient support (idle vs pressed)
   - Brightness control
   - Effect presets (rainbow, breathing, chase, etc.)
   - Player LED (PLED) outputs

4. **Macro system** — Button-triggered input sequences:
   - Per-button macros
   - 3 modes: press, hold-repeat, toggle
   - Up to 30 inputs per macro
   - Microsecond timing
   - Exclusive/interruptible flag

### Important (commonly requested)

5. **Backup/Restore** — JSON export/import of all settings:
   - Selective per-component checkboxes
   - Versioned format with migration
   - Timestamped filenames

6. **More hotkey actions** — They have 22+ actions vs our 7. Missing:
   - Home/Capture button trigger
   - SOCD mode cycling
   - 4-way joystick toggle
   - Stick invert toggle
   - Touchpad button
   - Per-button injection (L1-L3, R1-R3, B1-B4 individual hotkeys)
   - Reboot to default

7. **Custom button layouts** — Visual button layout editor. We only have a flat list.

8. **Keyboard mapping** — Map buttons to keyboard keys (gamepad-as-keyboard mode).

9. **Peripheral mapping (I2C)** — Configure analog sensors, character displays, custom I2C blocks.

### Nice-to-have

10. **Stick curves / sensitivity** — Beyond just deadzone
11. **Trigger curves** — L2/R2 analog response shaping
12. **Input History visualization** — Recording/playback of input history
13. **Wii extension support** — Nunchuk, Classic, Guitar, Drum, Turntable, Taiko
14. **Multiple input mode groups** — "Primary" vs "Mini" mode categories

## Architecture Comparison

| | Joypad OS | GP2040-CE |
|---|-----------|-----------|
| Framework | Vanilla ES6 | React + Bootstrap |
| State | Class instances | Formik + Context |
| Validation | Inline | Yup schema |
| i18n | Hardcoded English | Full i18next translations |
| Styling | Custom CSS | React Bootstrap components |
| Build | None (raw ES modules) | Vite + npm bundle |
| Transport | USB Serial + BLE NUS | USB Serial only |

GP2040-CE has heavier tooling but more polish. Joypad OS is simpler/lighter and supports BLE.

## Recommended Roadmap

### Phase 1: Foundation (highest impact)

1. **Add-on system framework** — A registry/lifecycle pattern for runtime-configurable features
   - Compile-time: which add-ons available
   - Flash storage: per-add-on enable/config
   - Web config: dynamic page generation per registered add-on
   - Firmware: each add-on hooks into router/event pipeline

2. **Backup/Restore** — Quick win, solves "I have to reconfigure everything"
   - Single `EXPORT.GET` and `IMPORT.SET` CDC command
   - Returns/accepts JSON blob of all flash contents
   - Web UI: download/upload buttons

3. **Macro system** — High-value feature for shmup/fighter players
   - Implement on top of existing combo/hotkey infrastructure
   - Add macro recorder UI

### Phase 2: Polish

4. **LED themes** — Color picker, per-button colors, simple effects
5. **More hotkey actions** — Add 10-15 more actions to existing combo system
6. **Stick curves** — Add to existing pad config

### Phase 3: Hardware features

7. **Display configuration** — Big undertaking, needs OLED driver + bitmap editor
8. **Peripheral I2C mapping** — Generic I2C device registry
9. **Keyboard mapping mode** — New USB output mode

## Notes

- GP2040-CE's biggest architectural advantage is the add-on system. Every "feature" is a self-contained add-on with its own page, config struct, and runtime hooks. We'd benefit from adopting this pattern.
- Their LED system is more polished but our universal-app trajectory may favor a simpler approach.
- Backup/restore should be straightforward and high-value to implement first.
