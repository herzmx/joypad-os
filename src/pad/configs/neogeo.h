// neogeo.h - NEOGEO/Supergun Configuration
// SPDX-License-Identifier: Apache-2.0
//
//
// ============================================================================
// Physical layout:
// ============================================================================
// Maps NEOGEO/Arcade classic buttons to arcade stick
//
//  ( )    
//   |      (B) (C ) (D ) ( )
//          (A) (K2) (K3) ( )
//

#ifndef PAD_CONFIG_NEOGEO_H
#define PAD_CONFIG_NEOGEO_H

#include "../pad_input.h"

// ============================================================================
// NEOGEO - BUTTON ONLY
// ============================================================================
// KB2040 pin mapping for NEOGEO controller/stick button mod.
// Active low buttons (pressed = GPIO low)

static const pad_device_config_t pad_config_neogeo = {
    .name = "NEOGEO",
    .active_high = false,   // Keys have pull-ups (pressed = low)

    // No I2C expanders
    .i2c_sda = PAD_PIN_DISABLED,
    .i2c_scl = PAD_PIN_DISABLED,

    // D-pad (GPIO 10, 19, 20, 18)
    .dpad_up    = 10,        // GPIO 10
    .dpad_down  = 19,        // GPIO 19
    .dpad_left  = 20,        // GPIO 20
    .dpad_right = 18,        // GPIO 18

    // Buttons (keys 8, 9, 11, 12)
    .b1 = 5,                 // A / Cross (GPIO 8)
    .b2 = 8,                 // B / Circle (GPIO 11)
    .b3 = 2,                 // X / Square (GPIO 9)
    .b4 = 3,                 // Y / Triangle (GPIO 12)

    .l1 = PAD_PIN_DISABLED,
    .r1 = 4,                 // LB / L1 (GPIO 4)
    .l2 = PAD_PIN_DISABLED,
    .r2 = 9,                 // RB / R1 (GPIO 9)

    // Meta buttons (key 10)
    .s1 = 7,                 // Select / Back (GPIO 7)
    .s2 = 6,                 // Start (GPIO 6)

    // No stick clicks
    .l3 = PAD_PIN_DISABLED,
    .r3 = PAD_PIN_DISABLED,

    // Aux buttons (unused)
    .a1 = PAD_PIN_DISABLED,
    .a2 = PAD_PIN_DISABLED,

    // Extra buttons (Key 2 unused)
    .l4 = PAD_PIN_DISABLED,
    .r4 = PAD_PIN_DISABLED,

    // No analog sticks
    .adc_lx = PAD_PIN_DISABLED,
    .adc_ly = PAD_PIN_DISABLED,
    .adc_rx = PAD_PIN_DISABLED,
    .adc_ry = PAD_PIN_DISABLED,

    .invert_lx = false,
    .invert_ly = false,
    .invert_rx = false,
    .invert_ry = false,
    .deadzone = 10,

    // NeoPixel on GPIO 17
    .led_pin = 17,
    .led_count = 1,
};

#endif // PAD_CONFIG_NEOGEO_H
