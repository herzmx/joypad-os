# usb2neogeo

USB/BT controllers to Neo Geo / SuperGun.

## Overview

Connects USB and Bluetooth controllers to Neo Geo consoles (AES/MVS) and SuperGun arcade boards via GPIO-based active-low output to a DB15 connector. Supports 7 button mapping profiles optimized for different arcade stick layouts and pad configurations. Uses open-drain logic to safely interface 3.3V GPIO with 5V Neo Geo hardware.

## Input

- [USB HID](../input/usb-hid.md), [XInput](../input/xinput.md), [Bluetooth](../input/bluetooth.md) controllers

## Output

[Neo Geo Output](../output/neogeo.md) -- Active-low GPIO via DB15 connector.

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | SIMPLE (1:1) |
| Player slots | 1 (shift on disconnect) |
| Max USB devices | 1 |
| Profile system | 7 profiles, saved to flash |

## Profiles

Hold **Select** for 2 seconds, then press **D-Pad Up/Down** to cycle.

| Profile | Description |
|---------|-------------|
| **Default** | Standard 1L6B layout |
| **Type A** | 1L6B aligned to right side of 1L8B stick |
| **Type B** | Neo Geo MVS 1L4B layout |
| **Type C** | Neo Geo MVS Big Red layout |
| **Type D** | Neo Geo MVS U4 layout |
| **Pad A** | AES pad, classic diamond (A/B/C/D on face buttons) |
| **Pad B** | AES pad, KOF/fighting style |

See the [Neo Geo adapter docs](../adapters/neogeo.md) for full per-profile button mapping tables with diagrams.

## Supported Boards

| Board | Build Command |
|-------|---------------|
| KB2040 | `make usb2neogeo_kb2040` |
| Pico | `make usb2neogeo_pico` |
| RP2040-Zero | `make usb2neogeo_rp2040zero` |

## Build and Flash

```bash
make usb2neogeo_kb2040
make flash-usb2neogeo_kb2040
```
