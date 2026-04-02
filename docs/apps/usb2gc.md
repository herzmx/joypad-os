# usb2gc

USB/BT controllers to GameCube/Wii console.

## Overview

Connects USB and Bluetooth controllers to a GameCube or Wii console via the joybus protocol. Supports 5 button mapping profiles, keyboard mode for PSO, copilot mode (merge multiple controllers), and rumble feedback. Requires 130MHz overclock for joybus timing.

## Input

- [USB HID](../input/usb-hid.md), [XInput](../input/xinput.md), [Bluetooth](../input/bluetooth.md) controllers
- USB keyboard (keyboard mode for Phantasy Star Online)
- USB mouse (mapped to analog stick)

## Output

[GameCube Output](../output/gamecube.md) -- joybus PIO protocol on a single port.

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | MERGE (all inputs blended to one output) |
| Player slots | 4 (fixed assignment) |
| Max USB devices | 4 |
| Clock speed | 130MHz (required for joybus) |
| Profile system | 5 profiles, saved to flash |

## Profiles

Hold **Select** for 2 seconds, then press **D-Pad Up/Down** to cycle. Controller rumbles and LED flashes to confirm. Profile persists across power cycles.

| Profile | Description |
|---------|-------------|
| **Default** | Standard mapping for most games |
| **SNES** | L/R as full analog press, Select mapped to Z |
| **SSBM** | Super Smash Bros. Melee competitive (light shield, multi-jump, 85% stick sensitivity) |
| **MKWii** | Mario Kart Wii (LB=wheelie, RB=drift, RT=item throw) |
| **Fighting** | 2D fighters (right stick disabled, LB=C-Up macro) |

See [GameCube Output](../output/gamecube.md) for full button mapping tables per profile.

## Key Features

- **Keyboard mode** -- Toggle with Scroll Lock or F14. Works with PSO and other GC keyboard games.
- **Copilot mode** -- Multiple controllers merge into one output. All inputs OR'd together.
- **Rumble** -- Forwarded to compatible controllers (Xbox, DualShock 3/4/5, Switch Pro, 8BitDo).
- **Adaptive triggers** -- DualSense L2/R2 analog mapped to GC L/R with configurable threshold.

## Supported Boards

| Board | Build Command |
|-------|---------------|
| KB2040 | `make usb2gc_kb2040` |
| RP2040-Zero | `make usb2gc_rp2040zero` |

## Build and Flash

```bash
make usb2gc_kb2040
make flash-usb2gc_kb2040
```
