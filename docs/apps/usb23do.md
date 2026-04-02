# usb23do

USB/BT controllers to 3DO Interactive Multiplayer.

## Overview

Connects USB and Bluetooth controllers to a 3DO console via the PBUS daisy-chain protocol. Supports up to 8 players, USB mouse as a 3DO mouse, extension passthrough for native 3DO controllers, and multiple button mapping profiles. Requires a bidirectional 3.3V-to-5V level shifter.

## Input

- [USB HID](../input/usb-hid.md), [XInput](../input/xinput.md), [Bluetooth](../input/bluetooth.md) controllers
- USB mouse (3DO mouse emulation)

## Output

[3DO Output](../output/3do.md) -- PBUS serial PIO protocol with daisy-chain support.

## Core Configuration

| Setting | Value |
|---------|-------|
| Routing mode | SIMPLE (1:1 controller to PBUS slot) |
| Player slots | 8 (shift on disconnect) |
| Max USB devices | 8 |
| Profile system | 3 profiles |

## Profiles

Hold **Select** for 2 seconds, then press **D-Pad Up/Down** to cycle.

| Profile | Description |
|---------|-------------|
| **Default** | SNES-style layout (B1=B, B2=C, B3=A) |
| **Fighting** | Way of the Warrior, SFII (face buttons = punches, shoulders = kicks) |
| **Shooter** | Doom, PO'ed (shoulders = fire, face = movement actions) |

See [3DO Output](../output/3do.md) for full button mapping tables per profile.

## Key Features

- **8-player support** -- Up to 8 USB controllers via USB hub, each mapped to a PBUS slot.
- **Extension passthrough** -- Native 3DO controllers connect in series after the USB adapter.
- **Mouse support** -- USB mouse emulates 3DO mouse for Myst, The Horde, Lemmings.
- **Device types** -- Joypad (2-byte), Joystick (9-byte), and Mouse (4-byte) PBUS reports.

## Supported Boards

| Board | Build Command |
|-------|---------------|
| RP2040-Zero | `make usb23do_rp2040zero` |

## Build and Flash

```bash
make usb23do_rp2040zero
make flash-usb23do_rp2040zero
```
