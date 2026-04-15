# Universal Controller App Consolidation

## Goal

Consolidate `controller_btusb`, `usb2usb`, `bt2usb`, and `controller` into a single universal app with compile-time board configs. All runtime-configurable via web config.

## Current Apps

| App | Input | Output | Router | BLE Role |
|-----|-------|--------|--------|----------|
| controller_btusb | GPIO, JoyWing | BLE + USB | Merge/Blend | Peripheral |
| usb2usb | USB Host (4 devices) | USB | Merge/Blend | None (or Central on Pico W) |
| bt2usb | BT Classic + BLE Central | USB | Merge/Blend | Central |
| controller | GPIO only | USB | Simple | None |

## What controller_btusb Already Has

- Configurable GPIO pad input (pins from flash)
- Configurable JoyWing I2C input (pins from flash)
- Configurable USB Host input (PIO-USB pin from flash, or MAX3421E compile-time)
- USB Device output (always, with mode switching)
- BLE Peripheral output (on supported boards)
- Router mode configurable from flash (Simple/Merge)
- D-pad mode configurable from flash + hotkeys
- Profiles with button remapping
- Hotkey combos with actions (Fn keys, d-pad mode, profile cycling)
- NeoPixel LED with mode colors, runtime pin config, disable option
- Speaker/buzzer config
- Full web config over USB Serial + BLE NUS
- Firmware update check + one-click OTA

## What's Missing for Full Consolidation

### 1. BT Classic/BLE Central Input (from bt2usb)
- bt2usb uses BTstack as BLE Central (scanning, connecting to controllers)
- controller_btusb uses BTstack as BLE Peripheral (advertising, accepting connections)
- **Challenge**: Can't be both Central and Peripheral simultaneously on most chips
- **Solution**: Compile-time flag `REQUIRE_BT_INPUT` vs `REQUIRE_BLE_OUTPUT`
- On Pico W: Could potentially do both (CYW43 supports dual role)
- On nRF52840/ESP32: BLE-only, pick one role

### 2. USB Host Input (from usb2usb)
- usb2usb supports 4 USB host devices with player management
- controller_btusb_abb already has USB host but single-device focused
- **Solution**: Already works — just needs `MAX_USB_DEVICES=4` and proper player slot config

### 3. Multi-Player Output (from usb2usb)
- usb2usb routes 4 inputs to separate player slots
- controller_btusb merges everything to 1 player
- **Solution**: Router mode configurable (already done — Simple vs Merge)

### 4. I2C Peer Mode (from usb2usb + controller)
- Master: sends status to slave controllers
- Slave: serves inputs to master via I2C
- **Solution**: Optional compile-time feature, not critical

### 5. UART Linking (from controller)
- Bidirectional controller linking over UART/QWIIC
- **Solution**: Optional compile-time feature, already has pin config in flash

### 6. Native USB Host/Device Auto-Detection
- Future: detect if connected to a computer (device mode) or has a controller plugged in (host mode)
- Needed for USB2GC-style boards where same port does both
- **Solution**: Runtime detection at boot, configurable override in web config

## Consolidation Strategy

### Phase 1: Compile-Time Board Configs (Recommended Start)
Keep `controller_btusb` as the universal app. Each board target sets defines:

```cmake
# usb2usb equivalent
target_compile_definitions(universal_feather PRIVATE
    APP_NAME="usb2usb"
    REQUIRE_USB_HOST=1
    MAX_USB_DEVICES=4
    DISABLE_BLE_OUTPUT=1
    CONFIG_PAD_INPUT=1
)

# bt2usb equivalent
target_compile_definitions(universal_pico_w PRIVATE
    APP_NAME="bt2usb"
    REQUIRE_BT_INPUT=1
    DISABLE_USB_HOST=1
    CONFIG_PAD_INPUT=1
)

# controller_btusb (current)
target_compile_definitions(universal_pico_w_controller PRIVATE
    APP_NAME="controller_btusb"
    REQUIRE_BLE_OUTPUT=1
    SENSOR_PAD=1
    SENSOR_JOYWING=1
    CONFIG_PAD_INPUT=1
)
```

### Phase 2: Shared Button Handler
Extract button event handling into a dispatch table:
- Single-click: Show mode info (all modes)
- Double-click: Cycle output mode (USB or BLE depending on what's active)
- Triple-click: Reset to default mode
- Long press: Clear bonds / disconnect (BT modes)

### Phase 3: Runtime Feature Detection
- Probe for MAX3421E at boot (already done)
- Probe for JoyWing at boot (already done)
- Check if BT hardware present
- Check if USB host has devices connected
- Auto-configure router based on what's actually present

## Binary Size Impact

Current approximate sizes (RP2040):
- controller_btusb_abb: ~360KB flash
- usb2usb: ~380KB flash
- bt2usb_pico_w: ~420KB flash (includes BTstack)

Universal app with all features compiled: ~450KB (well within 2MB flash).
Per-board builds with unused features disabled: similar to current sizes.

## Migration Path

1. **Now**: controller_btusb is the development target, other apps maintained separately
2. **Next**: Add USB host multi-device support to controller_btusb
3. **Then**: Add BT Central input option (compile-time flag)
4. **Finally**: Deprecate separate usb2usb/bt2usb apps, point all board targets at universal app
