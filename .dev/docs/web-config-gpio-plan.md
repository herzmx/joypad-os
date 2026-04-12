# Runtime GPIO Pin Configuration via Web Config

## Overview

Move controller pad GPIO pin mappings from compile-time (`pad_device_config_t` in header files) to runtime flash storage, configurable through the web config UI over USB Serial or BLE NUS.

---

## 1. Current State

### Pad Config (Compile-Time)
- `pad_device_config_t` in `src/pad/pad_input.h` — ~200 byte struct
- 22 digital button pins, 4 ADC channels, I2C expander, LEDs, speaker, display
- Configs defined as `static const` in `src/pad/configs/*.h` (fisherprice, alpakka, abb, macropad)
- Selected via `#ifdef CONTROLLER_TYPE_*` at compile time

### Flash Storage
- Dual-sector journaled design, 4KB sectors, 256-byte pages
- `flash_t` struct: 256 bytes (header, settings, 4 custom profiles)
- No pad config storage exists

### CDC Protocol
- JSON-over-binary-framed packets, 512 byte max payload
- Commands: INFO, MODE, PROFILE, SETTINGS, INPUT.STREAM, BLE.MODE, etc.
- Works over USB CDC and BLE NUS (via `ble_nus.c` bridge)

### Web Config UI
- `tools/web-config/` — Vite + vanilla JS
- Transport-agnostic `CDCProtocol` class (Web Serial + Web Bluetooth)
- Cards: Device info, USB/BLE mode, Profiles, Input Test, Advanced

---

## 2. Firmware Changes

### 2.1 Flash Schema — `pad_config_flash_t` (256 bytes)

```c
typedef struct {
    uint32_t magic;             // 4B — 0x50414443 ("PADC")
    uint32_t sequence;          // 4B — journaling
    char name[16];              // 16B — inline name (no pointer)
    uint8_t flags;              // 1B — active_high (bit 0), dpad_toggle_invert (bit 1)
    int8_t i2c_sda;             // 1B
    int8_t i2c_scl;             // 1B
    uint8_t deadzone;           // 1B
    int16_t buttons[22];        // 44B — dpad UDLR, b1-b4, l1/r1/l2/r2, s1/s2, l3/r3, a1/a2, l4/r4
    int16_t dpad_toggle;        // 2B
    int8_t adc_channels[4];     // 4B — lx, ly, rx, ry
    uint8_t adc_invert;         // 1B — bitmask (bits 0-3)
    int8_t led_pin;             // 1B
    uint8_t led_count;          // 1B
    int8_t speaker_pin;         // 1B
    int8_t speaker_enable_pin;  // 1B
    uint8_t reserved[170];      // pad to 256B (room for LED colors, display in Phase 4)
} pad_config_flash_t;
```

**Storage**: New flash sector before existing dual-sector journal. Same journaling pattern (16 slots, sequence number).

### 2.2 Runtime Config Loading

```c
// In app_init():
const pad_device_config_t* config = pad_config_load_runtime();
if (!config) config = &PAD_CONFIG;  // Fall back to compile-time default
pad_input_add_device(config);
```

New files: `src/pad/pad_config_runtime.h/.c` — converts between flash and runtime structs.

### 2.3 New CDC Commands

| Command | Direction | Description |
|---------|-----------|-------------|
| `PAD.CONFIG.GET` | ← Device | Read current config (source: "flash" or "default") |
| `PAD.CONFIG.SET` | → Device | Write new config to flash, triggers reboot |
| `PAD.CONFIG.RESET` | → Device | Delete flash config, revert to compile-time default |
| `PAD.CONFIG.PINS` | ← Device | List available GPIO/ADC/I2C pins for this board |

GET/SET payloads are ~400 bytes JSON, fits within 512B `CDC_MAX_PAYLOAD`.

Guarded with `#ifdef CONFIG_PAD_INPUT` — only compiled for controller apps.

---

## 3. Web Config UI Changes

### 3.1 New Card: "GPIO Pin Configuration"

- Config name text input
- Source indicator ("Custom (Flash)" / "Default: Fisher Price V2")
- Active High/Low toggle
- Button pin assignment table — dropdown per button (Disabled, GPIO 0-29, I2C 100-115/200-215)
- D-pad toggle pin + invert
- ADC stick assignments (4 dropdowns: Disabled, ADC 0-3) + invert checkboxes
- Deadzone slider (0-127)
- I2C expander SDA/SCL dropdowns
- LED pin + count
- Save button (with reboot warning)
- Reset to Default button

### 3.2 Pin Conflict Detection (Client-Side)

- Highlight duplicate GPIO assignments in red
- Warn if ADC pin (26-29) used as both digital and ADC
- Warn if I2C SDA/SCL overlap with button pins

### 3.3 Transport — No Changes Needed

Existing `CDCProtocol` class handles arbitrary JSON commands over both transports.

---

## 4. Data Flow

```
Web UI → CDCProtocol.sendCommand('PAD.CONFIG.SET', {...})
       → Binary frame [0xAA][len][CMD][seq][JSON][CRC16]
       → USB CDC endpoint / BLE NUS RX
       → cdc_commands.c: cmd_pad_config_set()
           → Parse JSON → pad_config_flash_t → flash write
           → Response: {"ok":true,"reboot":true}
           → Deferred reboot (50ms)
       → Device reboots
       → app_init() → pad_config_load_runtime() → pad_input_add_device()
```

---

## 5. Backward Compatibility

- No flash pad config = existing behavior (compile-time default)
- Flash magic `0x50414443` distinguishes from settings magic `0x47435052`
- Compile-time configs (`src/pad/configs/*.h`) remain as defaults + documentation
- `SETTINGS.RESET` also clears pad config sector
- Existing devices upgrading: pad config sector is unprogrammed (0xFF) = "no config"

---

## 6. Phased Implementation

### Phase 1: Firmware Foundation
- `pad_config_flash_t` struct
- Flash sector read/write
- `pad_config_from_flash()` / `pad_config_to_flash()` conversion
- `pad_config_load_runtime()` in app init path

### Phase 2: CDC Commands
- `PAD.CONFIG.GET/SET/RESET/PINS` handlers
- Command dispatch registration
- Conditional compilation guard

### Phase 3: Web Config UI
- `PadConfigUI` class in `pad-config.js`
- Pin assignment form with conflict detection
- Load/save/reset wired to CDC protocol
- Reboot flow handling

### Phase 4: Extended Config (Future)
- LED color configuration per-button
- LED button mapping
- Display/QWIIC config
- Visual pin diagram (SVG)
- Config export/import (JSON file)
