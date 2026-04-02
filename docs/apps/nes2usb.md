# nes2usb

NES controller to USB HID gamepad.

## Overview

Reads a native NES controller via PIO-based shift register protocol and outputs as a USB HID gamepad. 60Hz polling with fractional timing correction for accurate cadence. Automatic connect/disconnect detection with 500ms debounce and stuck-button prevention.

## Input

[NES Input](../input/nes.md) -- PIO shift register protocol at 1MHz instruction clock. CLK (GPIO 5), LATCH (GPIO 6), DATA (GPIO 8).

## Output

[USB Device Output](../output/usb-device.md) -- USB HID gamepad with multiple emulation modes.

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | SIMPLE (1:1) |
| Player slots | 1 (fixed) |
| Polling rate | 60Hz (fractional correction for accuracy) |
| Profile system | None |

## Key Features

- **Auto-detection** -- Controller connected/disconnected detected via DATA line state.
- **USB output modes** -- SInput, XInput, PS3, PS4, Switch, Keyboard/Mouse. Double-click button to cycle, triple-click to reset.
- **Web config** -- [config.joypad.ai](https://config.joypad.ai).

## Supported Boards

| Board | Build Command |
|-------|---------------|
| KB2040 | `make nes2usb_kb2040` |
| Pico W | `make nes2usb_pico_w` |

## Build and Flash

```bash
make nes2usb_kb2040
make flash-nes2usb_kb2040
```
