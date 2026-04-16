# Bluetooth Pro Controller Output Mode

Implement Switch Pro Controller emulation over Classic Bluetooth HID, allowing controller_btusb (and other apps) to advertise as a Nintendo Switch Pro Controller for native Switch console pairing.

## Reference Implementation

HOJA-LIB-RP2040 has a complete working implementation:
- `src/hal/rp2040/bluetooth_hal.c` — BTstack Classic BT HID setup (~540 lines)
- `src/switch/switch_commands.c` — Subcommand processor (~660 lines)
- `src/switch/switch_spi.c` — Fake SPI flash emulation for calibration data (~500 lines)
- `src/switch/switch_haptics.c` — Rumble decoding
- `src/switch/switch_motion.c` — IMU report packing
- `src/switch/switch_analog.c` — Stick value scaling

## Platform Support

- **Pico W (CYW43)**: Yes — supports Classic BT
- **nRF52840**: No — BLE only
- **ESP32-S3**: No — BLE only (in this codebase)
- **USB BT Dongle**: Yes — any board with USB host can use a dongle

So compile-time flag: `REQUIRE_BT_CLASSIC_OUTPUT`. Only enable on targets that can do Classic BT.

## Identification

Switch consoles look for specific VID/PID + HID descriptor:
- Name: `Pro Controller`
- VID: `0x057E` (Nintendo)
- PID: `0x2009` (Switch Pro Controller)
- Class of Device: `0x2508` (Peripheral / Joystick)
- HID Report Descriptor: must match Switch Pro Controller exactly

## Architecture

### New Files
- `src/bt/bt_output/bt_output_switch.c` — Switch Pro mode implementation (Classic BT)
- `src/bt/bt_output/bt_output_switch.h`
- `src/bt/bt_output/switch_subcmd.c` — Subcommand processor (port from HOJA)
- `src/bt/bt_output/switch_spi_flash.c` — Fake SPI flash for calibration
- `src/usb/usbd/descriptors/switch_pro_bt_hid.h` — Switch Pro BT HID descriptor

### Modified Files
- `src/bt/ble_output/ble_output.h` — Rename to `bt_output.h`? Or add separate Classic BT output module
- `src/core/services/storage/flash.h` — Add `BT_MODE_SWITCH_PRO` to mode enum
- `src/CMakeLists.txt` — Conditionally add Classic BT sources for Pico W targets
- `tools/web-config/src/components/bt-output.js` — Add Pro Controller option to mode dropdown

## Implementation Phases

### Phase 1: Classic BT HID Device Foundation
1. Add BTstack Classic BT HID Device setup (parallel to existing BLE HID Device)
2. `gap_set_class_of_device(0x2508)`, `gap_set_local_name("Pro Controller")`
3. L2CAP setup for HID Control (PSM 0x11) and HID Interrupt (PSM 0x13)
4. SDP record with Switch Pro VID/PID and HID descriptor
5. Verify pairing with a Switch console (or scrcpy/macOS shows correct name)

### Phase 2: Switch Pro HID Descriptor
1. Port `switch_bt_report_descriptor` from HOJA bluetooth_hal.c
2. Standard input report (0x30): buttons, sticks, IMU
3. Subcommand reply report (0x21): contains subcommand response
4. NFC/IR data report (0x31): unused but required for descriptor compliance

### Phase 3: Subcommand Handler
Switch console sends ~10-15 subcommands during pairing. Must respond correctly to:
- `0x01` Bluetooth manual pairing
- `0x02` Request device info (returns VID/PID, MAC, controller type)
- `0x03` Set input report mode
- `0x04` Trigger buttons elapsed time
- `0x08` Set shipment low power state
- `0x10` SPI flash read (calibration data — see Phase 4)
- `0x21` Set NFC/IR config
- `0x22` Set NFC/IR state
- `0x30` Set player LED
- `0x40` Enable IMU
- `0x48` Enable rumble

Port `switch_commands.c` from HOJA (~660 lines).

### Phase 4: Fake SPI Flash for Calibration
Switch console reads calibration data from "SPI flash" addresses. Must respond with valid factory calibration:
- `0x6000-0x603F`: Serial number
- `0x6020-0x6037`: Factory calibration (sticks)
- `0x6080-0x6097`: User calibration markers
- `0x8010-0x8025`: User stick calibration
- `0x6050-0x605B`: Body and button colors

Port `switch_spi.c` from HOJA (~500 lines). Most addresses return canned responses.

### Phase 5: Input Reports
Standard input report format (Report ID 0x30, 49 bytes):
- Timer (1 byte)
- Battery + connection info (1 byte)
- Buttons (3 bytes)
- Left stick (3 bytes, packed 12-bit)
- Right stick (3 bytes, packed 12-bit)
- Vibrator data (1 byte)
- IMU data (3 frames of 12 bytes)

Need to pack our `input_event_t` into this format. Port `switch_analog.c` for stick scaling and IMU framing.

### Phase 6: Rumble
Switch sends rumble commands as 8-byte packets in input report. Decode amplitude/frequency, output via `output_feedback_t.rumble_left/right` (already in our infrastructure).

### Phase 7: Web Config Integration
- Add "Pro Controller" to BLE/BT output mode dropdown
- Hide on platforms that don't support Classic BT
- Show pairing instructions in UI when this mode is selected

## Naming Cleanup

Currently `ble_output.*` handles only BLE Peripheral. Adding Classic BT means we need:
- `ble_output.*` — BLE GATT HID (Standard, Xbox)
- `bt_output_switch.*` — Classic BT HID (Pro Controller)
- Or merge: `bt_output.*` covers both, with mode determining transport

Rename to `bt_output.*` and add per-mode transport selection cleaner long-term.

## Mode Enum Update

```c
typedef enum {
    BT_MODE_STANDARD = 0,        // BLE GATT HID composite
    BT_MODE_XBOX,                // BLE GATT HID Xbox
    BT_MODE_SINPUT,              // BLE SInput (future, also Classic via dongle)
    BT_MODE_SWITCH_PRO,          // Classic BT Switch Pro Controller (Pico W only)
    BT_MODE_COUNT
} bt_output_mode_t;
```

## Risks / Caveats

1. **Pairing complexity**: Switch pairing is finicky. Subcommand responses must be correct.
2. **Address rotation**: Switch may store the BD address on first pairing; need stable address.
3. **Disconnect/reconnect**: Switch expects specific behavior on wakeup.
4. **Coexistence with BLE**: On Pico W, dual-role Classic + BLE works but uses more radio time.
5. **Authentication**: Some Switch firmware versions require Joy-Con-style cryptographic authentication. HOJA bypasses this — needs verification.

## Estimated Effort

- Phase 1-2: 1-2 sessions (BTstack setup + HID descriptor)
- Phase 3-4: 2-3 sessions (subcommands + SPI flash)
- Phase 5-6: 1-2 sessions (input reports + rumble)
- Phase 7: 1 session (web config)
- Total: ~6-8 focused sessions

## Testing

- Pico W with controller_btusb_pico_w build
- Switch console (Lite or original)
- macOS BT inspector (verify advertising name + class of device)
- HOJA reference for byte-level comparison of subcommand responses
