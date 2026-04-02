// profiles.h - LodgeNet2N64 Profile Definitions
//
// Button mapping profiles for LodgeNet to N64 adapter.
// LodgeNet controllers are already N64/GC/SNES format,
// so the default profile is a 1:1 passthrough.

#ifndef LODGENET2N64_PROFILES_H
#define LODGENET2N64_PROFILES_H

#include "core/services/profiles/profile.h"
#include "native/device/n64/n64_buttons.h"

// ============================================================================
// PROFILE: Default - Direct Passthrough
// ============================================================================
// LodgeNet N64 controller input is already mapped to JP_BUTTON_* by
// lodgenet_host, so this is a 1:1 passthrough to N64 output buttons.

// LodgeNet input driver maps N64 buttons to JP_BUTTON_* as follows:
//   A→B1, B→B3, C-Down→B2, C-Left→B4, C-Up→L3, C-Right→R3
//   Z→R1, L→L2, R→R2, Start→S2
// N64 output device expects buttons via N64_BUTTON_* aliases:
//   A=B1, B=B3, C-Down=B2, C-Left=B4, C-Up=R2, C-Right=R3
//   Z=L2, L=L1, R=R1, Start=S2
// Profile remaps the mismatched ones (Z, L, R, C-Up).

static const button_map_entry_t lodgenet_n64_default_map[] = {
    // Face buttons (match directly)
    MAP_BUTTON(JP_BUTTON_B1, N64_BUTTON_A),      // A -> A (B1->B1)
    MAP_BUTTON(JP_BUTTON_B3, N64_BUTTON_B),      // B -> B (B3->B3)

    // C-buttons
    MAP_BUTTON(JP_BUTTON_B2, N64_BUTTON_CD),     // C-Down (B2->B2, direct)
    MAP_BUTTON(JP_BUTTON_B4, N64_BUTTON_CL),     // C-Left (B4->B4, direct)
    MAP_BUTTON(JP_BUTTON_L3, N64_BUTTON_CU),     // C-Up: LN maps to L3, N64 wants R2
    MAP_BUTTON(JP_BUTTON_R3, N64_BUTTON_CR),     // C-Right (R3->R3, direct)

    // Shoulders + trigger (all remapped)
    MAP_BUTTON(JP_BUTTON_R1, N64_BUTTON_Z),      // Z: LN maps to R1, N64 wants L2
    MAP_BUTTON(JP_BUTTON_L2, N64_BUTTON_L),      // L: LN maps to L2, N64 wants L1
    MAP_BUTTON(JP_BUTTON_R2, N64_BUTTON_R),      // R: LN maps to R2, N64 wants R1

    // System
    MAP_BUTTON(JP_BUTTON_S2, N64_BUTTON_START),  // Start -> Start (S2->S2, direct)
    MAP_BUTTON(JP_BUTTON_A1, N64_BUTTON_START),  // Menu (Home) -> Start

    // LodgeNet extra buttons - disabled for N64 output
    MAP_DISABLED(JP_BUTTON_S1),   // Select
    MAP_DISABLED(JP_BUTTON_L1),   // (unmapped)
    MAP_DISABLED(JP_BUTTON_A2),   // Order
    MAP_DISABLED(JP_BUTTON_A4),   // Reset
    MAP_DISABLED(JP_BUTTON_L4),   // Hash
    MAP_DISABLED(JP_BUTTON_R4),   // Star
};

static const profile_t lodgenet_n64_profile_default = {
    .name = "default",
    .description = "Direct LodgeNet passthrough to N64",
    .button_map = lodgenet_n64_default_map,
    .button_map_count = sizeof(lodgenet_n64_default_map) / sizeof(lodgenet_n64_default_map[0]),
    .l2_behavior = TRIGGER_PASSTHROUGH,
    .r2_behavior = TRIGGER_PASSTHROUGH,
    .l2_threshold = 128,
    .r2_threshold = 128,
    .l2_analog_value = 0,
    .r2_analog_value = 0,
    .left_stick_sensitivity = 1.0f,
    .right_stick_sensitivity = 1.0f,
    .left_stick_modifiers = NULL,
    .left_stick_modifier_count = 0,
    .right_stick_modifiers = NULL,
    .right_stick_modifier_count = 0,
    .adaptive_triggers = false,
    .socd_mode = SOCD_PASSTHROUGH,
};

// ============================================================================
// PROFILE SET
// ============================================================================

static const profile_t lodgenet_n64_profiles[] = {
    lodgenet_n64_profile_default,
};

static const profile_set_t n64_profile_set = {
    .profiles = lodgenet_n64_profiles,
    .profile_count = sizeof(lodgenet_n64_profiles) / sizeof(lodgenet_n64_profiles[0]),
    .default_index = 0,
};

#endif // LODGENET2N64_PROFILES_H
