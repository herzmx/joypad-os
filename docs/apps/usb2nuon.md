# usb2nuon

USB/BT controllers to Nuon DVD player.

## Overview

Connects USB and Bluetooth controllers to a Nuon-enhanced DVD player via the Polyface serial protocol. Supports spinner emulation for Tempest 3000 using a USB mouse, and In-Game Reset (IGR) to return to the DVD menu or power off without getting up.

## Input

- [USB HID](../input/usb-hid.md), [XInput](../input/xinput.md), [Bluetooth](../input/bluetooth.md) controllers
- USB mouse (spinner emulation for Tempest 3000)

## Output

[Nuon Output](../output/nuon.md) -- Polyface PIO protocol.

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | SIMPLE (1:1) |
| Player slots | 1 (shift on disconnect) |
| Max USB devices | 1 |
| Profile system | Yes |
| Spinner support | Right stick / mouse X-axis to spinner |

## Key Features

- **Spinner emulation** -- USB mouse X-axis maps to Nuon spinner rotation. Left click maps to fire. Optimized for Tempest 3000.
- **In-Game Reset (IGR)** -- Hold L1 + R1 + Start + Select:
  - Tap (release before 2s): sends Stop (returns to DVD menu)
  - Hold 2+ seconds: sends Power (powers off the player)
- **Profiles** -- Hold Select 2s, then D-Pad Up/Down to cycle.

## Supported Boards

| Board | Build Command |
|-------|---------------|
| KB2040 | `make usb2nuon_kb2040` |

## Build and Flash

```bash
make usb2nuon_kb2040
make flash-usb2nuon_kb2040
```
