// wii_host.c - Native Wii extension controller host driver (HW I2C transport)

#include "wii_host.h"
#include "lib/wii_ext/wii_ext.h"
#include "core/router/router.h"
#include "core/input_event.h"
#include "core/buttons.h"
#include "core/services/leds/leds.h"
#include "core/services/profiles/profile.h"
#include "platform/platform_i2c.h"
#include "pico/time.h"
#include <stdio.h>
#include <string.h>

// Device address range for native inputs. SNES/LodgeNet use 0xF0+, N64/3DO
// 0xE0+, GC/UART 0xD0+. Wii claims 0xC0+ per the plan.
#define WII_DEV_ADDR_BASE       0xC0

// Poll / retry cadence.
#define WII_POLL_INTERVAL_US    2000      // ~500 Hz when connected
#define WII_RETRY_INTERVAL_US   250000    // 250 ms when hunting for a slave

// ---- State ------------------------------------------------------------------

static bool             initialized = false;
static uint8_t          pin_sda = WII_PIN_SDA;
static uint8_t          pin_scl = WII_PIN_SCL;
static platform_i2c_t   bus = NULL;
static wii_ext_t        ext;

static uint32_t         last_poll_us = 0;
static uint32_t         last_retry_us = 0;
static bool             prev_connected = false;
static uint32_t         prev_buttons = 0;
static uint64_t         prev_analog  = 0;

// Profile-cycle hotkey state: MINUS + DU/DD held ≥ 2s triggers cycle.
#define WII_HOTKEY_HOLD_US   2000000
static uint32_t         hotkey_combo_start_us = 0;
static uint32_t         hotkey_combo_mask     = 0;  // which combo is being tracked
static bool             hotkey_fired          = false;

// LED scan-blink state (toggles each retry while hunting for a slave).
static bool             led_scan_state        = false;

// Per-accessory LED colors (dim so the status LED doesn't overpower the
// player-index indication layered on top by core/services/leds).
#define LED_WII_NUNCHUCK_R    0
#define LED_WII_NUNCHUCK_G   16
#define LED_WII_NUNCHUCK_B   24
#define LED_WII_CLASSIC_R     0
#define LED_WII_CLASSIC_G     0
#define LED_WII_CLASSIC_B    40

// ---- wii_ext transport vtable (thin wrappers over platform_i2c) -------------

static int io_write(void *ctx, uint8_t addr, const uint8_t *data, uint16_t len) {
    return platform_i2c_write((platform_i2c_t)ctx, addr, data, len);
}
static int io_read(void *ctx, uint8_t addr, uint8_t *data, uint16_t len) {
    return platform_i2c_read((platform_i2c_t)ctx, addr, data, len);
}
static void io_delay(uint32_t us) {
    busy_wait_us(us);
}

static wii_ext_transport_t ext_transport;

// ---- Event mapping ----------------------------------------------------------

static void map_nunchuck(const wii_ext_state_t *s, input_event_t *ev) {
    if (s->buttons & WII_BTN_C) ev->buttons |= JP_BUTTON_B1;
    if (s->buttons & WII_BTN_Z) ev->buttons |= JP_BUTTON_B2;

    ev->analog[ANALOG_LX] = (uint8_t)(s->analog[WII_AXIS_LX] >> 2);
    // Nunchuck native Y is Y-up = 255; invert to HID's 0=up convention.
    ev->analog[ANALOG_LY] = (uint8_t)(255 - (s->analog[WII_AXIS_LY] >> 2));
    ev->analog[ANALOG_RX] = 128;
    ev->analog[ANALOG_RY] = 128;
    ev->analog[ANALOG_L2] = 0;
    ev->analog[ANALOG_R2] = 0;

    ev->layout = LAYOUT_WII_NUNCHUCK;
    ev->button_count = 2;

    if (s->has_accel) {
        ev->accel[0] = s->accel[0];
        ev->accel[1] = s->accel[1];
        ev->accel[2] = s->accel[2];
        ev->has_motion = true;
        ev->accel_range = 2000;
    }
}

// Guitar (GH3 + GHWT): frets on face buttons, strum on d-pad, whammy on RT.
static void map_guitar(const wii_ext_state_t *s, input_event_t *ev) {
    if (s->buttons & WII_BTN_GH_GREEN)      ev->buttons |= JP_BUTTON_B1;
    if (s->buttons & WII_BTN_GH_RED)        ev->buttons |= JP_BUTTON_B2;
    if (s->buttons & WII_BTN_GH_YELLOW)     ev->buttons |= JP_BUTTON_B3;
    if (s->buttons & WII_BTN_GH_BLUE)       ev->buttons |= JP_BUTTON_B4;
    if (s->buttons & WII_BTN_GH_ORANGE)     ev->buttons |= JP_BUTTON_L1;
    if (s->buttons & WII_BTN_GH_STRUM_UP)   ev->buttons |= JP_BUTTON_DU;
    if (s->buttons & WII_BTN_GH_STRUM_DOWN) ev->buttons |= JP_BUTTON_DD;
    if (s->buttons & WII_BTN_MINUS)         ev->buttons |= JP_BUTTON_S1;
    if (s->buttons & WII_BTN_PLUS)          ev->buttons |= JP_BUTTON_S2;

    ev->analog[ANALOG_LX] = (uint8_t)(s->analog[WII_AXIS_LX] >> 2);
    ev->analog[ANALOG_LY] = (uint8_t)(255 - (s->analog[WII_AXIS_LY] >> 2));
    ev->analog[ANALOG_RX] = 128;
    ev->analog[ANALOG_RY] = 128;
    ev->analog[ANALOG_L2] = 0;
    ev->analog[ANALOG_R2] = (uint8_t)(s->analog[WII_AXIS_RT] >> 2);  // whammy

    ev->layout = LAYOUT_WII_GUITAR;
    ev->button_count = 5;
}

// Drums: pads on face/shoulder buttons, velocity in event->pressure[].
static void map_drums(const wii_ext_state_t *s, input_event_t *ev) {
    if (s->buttons & WII_BTN_DRUM_RED)    ev->buttons |= JP_BUTTON_B1;
    if (s->buttons & WII_BTN_DRUM_YELLOW) ev->buttons |= JP_BUTTON_B3;
    if (s->buttons & WII_BTN_DRUM_BLUE)   ev->buttons |= JP_BUTTON_B4;
    if (s->buttons & WII_BTN_DRUM_GREEN)  ev->buttons |= JP_BUTTON_B2;
    if (s->buttons & WII_BTN_DRUM_ORANGE) ev->buttons |= JP_BUTTON_L1;
    if (s->buttons & WII_BTN_DRUM_BASS)   ev->buttons |= JP_BUTTON_R1;
    if (s->buttons & WII_BTN_MINUS)       ev->buttons |= JP_BUTTON_S1;
    if (s->buttons & WII_BTN_PLUS)        ev->buttons |= JP_BUTTON_S2;

    // Velocity → pressure[] — same slot layout as DualShock 3.
    // DS3 order: up, right, down, left, l2, r2, l1, r1, triangle, circle, cross, square
    // We map drum pads onto that approximate order via the face/shoulder
    // indices — PS3 mode consumers see pressure per-button.
    ev->pressure[10] = s->pad_velocity[WII_DRUM_PAD_RED];    // cross
    ev->pressure[8]  = s->pad_velocity[WII_DRUM_PAD_YELLOW]; // triangle
    ev->pressure[9]  = s->pad_velocity[WII_DRUM_PAD_BLUE];   // circle
    ev->pressure[11] = s->pad_velocity[WII_DRUM_PAD_GREEN];  // square
    ev->pressure[6]  = s->pad_velocity[WII_DRUM_PAD_ORANGE]; // l1 (cymbal)
    ev->pressure[7]  = s->pad_velocity[WII_DRUM_PAD_BASS];   // r1
    ev->has_pressure = true;

    ev->analog[ANALOG_LX] = 128;
    ev->analog[ANALOG_LY] = 128;
    ev->analog[ANALOG_RX] = 128;
    ev->analog[ANALOG_RY] = 128;
    ev->analog[ANALOG_L2] = 0;
    ev->analog[ANALOG_R2] = 0;

    ev->layout = LAYOUT_WII_DRUMS;
    ev->button_count = 6;
}

// DJ Hero turntable: rotations on sticks, crossfader on L2, effects on R2.
static void map_turntable(const wii_ext_state_t *s, input_event_t *ev) {
    if (s->buttons & WII_BTN_DJ_LEFT_GREEN)  ev->buttons |= JP_BUTTON_B1;
    if (s->buttons & WII_BTN_DJ_LEFT_RED)    ev->buttons |= JP_BUTTON_B2;
    if (s->buttons & WII_BTN_DJ_LEFT_BLUE)   ev->buttons |= JP_BUTTON_B3;
    if (s->buttons & WII_BTN_DJ_RIGHT_GREEN) ev->buttons |= JP_BUTTON_B4;
    if (s->buttons & WII_BTN_DJ_RIGHT_RED)   ev->buttons |= JP_BUTTON_L1;
    if (s->buttons & WII_BTN_DJ_RIGHT_BLUE)  ev->buttons |= JP_BUTTON_R1;
    if (s->buttons & WII_BTN_DJ_EUPHORIA)    ev->buttons |= JP_BUTTON_A1;
    if (s->buttons & WII_BTN_MINUS)          ev->buttons |= JP_BUTTON_S1;
    if (s->buttons & WII_BTN_PLUS)           ev->buttons |= JP_BUTTON_S2;

    ev->analog[ANALOG_LX] = (uint8_t)(s->analog[WII_AXIS_LX] >> 2);
    ev->analog[ANALOG_LY] = 128;
    ev->analog[ANALOG_RX] = (uint8_t)(s->analog[WII_AXIS_RX] >> 2);
    ev->analog[ANALOG_RY] = 128;
    ev->analog[ANALOG_L2] = (uint8_t)(s->analog[WII_AXIS_LT] >> 2);  // crossfader
    ev->analog[ANALOG_R2] = (uint8_t)(s->analog[WII_AXIS_RT] >> 2);  // effects dial

    ev->layout = LAYOUT_WII_TURNTABLE;
    ev->button_count = 6;
}

// Taiko: drum surfaces on face buttons.
static void map_taiko(const wii_ext_state_t *s, input_event_t *ev) {
    if (s->buttons & WII_BTN_TAIKO_L_FACE) ev->buttons |= JP_BUTTON_B1;
    if (s->buttons & WII_BTN_TAIKO_R_FACE) ev->buttons |= JP_BUTTON_B2;
    if (s->buttons & WII_BTN_TAIKO_L_RIM)  ev->buttons |= JP_BUTTON_B3;
    if (s->buttons & WII_BTN_TAIKO_R_RIM)  ev->buttons |= JP_BUTTON_B4;
    if (s->buttons & WII_BTN_MINUS)        ev->buttons |= JP_BUTTON_S1;
    if (s->buttons & WII_BTN_PLUS)         ev->buttons |= JP_BUTTON_S2;

    ev->analog[ANALOG_LX] = 128;
    ev->analog[ANALOG_LY] = 128;
    ev->analog[ANALOG_RX] = 128;
    ev->analog[ANALOG_RY] = 128;
    ev->analog[ANALOG_L2] = 0;
    ev->analog[ANALOG_R2] = 0;

    ev->layout = LAYOUT_WII_TAIKO;
    ev->button_count = 4;
}

// uDraw tablet: absolute tablet position surfaces via touch[0].
static void map_udraw(const wii_ext_state_t *s, input_event_t *ev) {
    if (s->buttons & WII_BTN_A) ev->buttons |= JP_BUTTON_B1;
    if (s->buttons & WII_BTN_B) ev->buttons |= JP_BUTTON_B2;
    if (s->buttons & WII_BTN_MINUS) ev->buttons |= JP_BUTTON_S1;
    if (s->buttons & WII_BTN_PLUS)  ev->buttons |= JP_BUTTON_S2;

    ev->analog[ANALOG_LX] = (uint8_t)(s->analog[WII_AXIS_LX] >> 2);
    ev->analog[ANALOG_LY] = (uint8_t)(s->analog[WII_AXIS_LY] >> 2);
    ev->analog[ANALOG_RX] = 128;
    ev->analog[ANALOG_RY] = 128;
    ev->analog[ANALOG_L2] = (uint8_t)(s->analog[WII_AXIS_LT] >> 2);
    ev->analog[ANALOG_R2] = 0;

    ev->touch[0].x = s->tablet_x;
    ev->touch[0].y = s->tablet_y;
    ev->touch[0].active = s->tablet_active;
    ev->has_touch = s->tablet_active;

    ev->layout = LAYOUT_WII_UDRAW;
    ev->button_count = 2;
}

// MotionPlus standalone: no buttons / analogs, just gyro passthrough.
static void map_motionplus(const wii_ext_state_t *s, input_event_t *ev) {
    ev->analog[ANALOG_LX] = 128;
    ev->analog[ANALOG_LY] = 128;
    ev->analog[ANALOG_RX] = 128;
    ev->analog[ANALOG_RY] = 128;
    ev->analog[ANALOG_L2] = 0;
    ev->analog[ANALOG_R2] = 0;

    if (s->has_gyro) {
        ev->gyro[0] = s->gyro[0];
        ev->gyro[1] = s->gyro[1];
        ev->gyro[2] = s->gyro[2];
        ev->has_motion = true;
        ev->gyro_range = 2000;  // normalized-to-fast-mode range
    }

    ev->layout = LAYOUT_WII_MOTIONPLUS;
    ev->button_count = 0;
}

static void map_classic(const wii_ext_state_t *s, input_event_t *ev) {
    // Wii Classic face layout, W3C positions (B1=south, B2=east, B3=west, B4=north):
    //       X        --> B4 (north)
    //     Y   A      --> Y=B3 (west), A=B2 (east)
    //       B        --> B1 (south)
    if (s->buttons & WII_BTN_A)     ev->buttons |= JP_BUTTON_B2;
    if (s->buttons & WII_BTN_B)     ev->buttons |= JP_BUTTON_B1;
    if (s->buttons & WII_BTN_X)     ev->buttons |= JP_BUTTON_B4;
    if (s->buttons & WII_BTN_Y)     ev->buttons |= JP_BUTTON_B3;
    if (s->buttons & WII_BTN_L)     ev->buttons |= JP_BUTTON_L1;
    if (s->buttons & WII_BTN_R)     ev->buttons |= JP_BUTTON_R1;
    if (s->buttons & WII_BTN_ZL)    ev->buttons |= JP_BUTTON_L2;
    if (s->buttons & WII_BTN_ZR)    ev->buttons |= JP_BUTTON_R2;
    if (s->buttons & WII_BTN_MINUS) ev->buttons |= JP_BUTTON_S1;
    if (s->buttons & WII_BTN_PLUS)  ev->buttons |= JP_BUTTON_S2;
    if (s->buttons & WII_BTN_HOME)  ev->buttons |= JP_BUTTON_A1;
    if (s->buttons & WII_BTN_DU)    ev->buttons |= JP_BUTTON_DU;
    if (s->buttons & WII_BTN_DD)    ev->buttons |= JP_BUTTON_DD;
    if (s->buttons & WII_BTN_DL)    ev->buttons |= JP_BUTTON_DL;
    if (s->buttons & WII_BTN_DR)    ev->buttons |= JP_BUTTON_DR;

    ev->analog[ANALOG_LX] = (uint8_t)(s->analog[WII_AXIS_LX] >> 2);
    ev->analog[ANALOG_LY] = (uint8_t)(255 - (s->analog[WII_AXIS_LY] >> 2));
    ev->analog[ANALOG_RX] = (uint8_t)(s->analog[WII_AXIS_RX] >> 2);
    ev->analog[ANALOG_RY] = (uint8_t)(255 - (s->analog[WII_AXIS_RY] >> 2));
    ev->analog[ANALOG_L2] = (uint8_t)(s->analog[WII_AXIS_LT] >> 2);
    ev->analog[ANALOG_R2] = (uint8_t)(s->analog[WII_AXIS_RT] >> 2);

    ev->layout = (s->type == WII_EXT_TYPE_CLASSIC_PRO)
                 ? LAYOUT_WII_CLASSIC_PRO : LAYOUT_WII_CLASSIC;
    ev->button_count = 4;
}

// ---- Public API -------------------------------------------------------------

void wii_host_init(void) {
    if (initialized) return;
    wii_host_init_pins(WII_PIN_SDA, WII_PIN_SCL);
}

void wii_host_init_pins(uint8_t sda, uint8_t scl) {
    pin_sda = sda;
    pin_scl = scl;

    platform_i2c_config_t cfg = {
        // Prefer I2C0 when GP12/GP13 (the board default pair on KB2040),
        // otherwise fall back to I2C1 which supports most other pairs.
        .bus     = (sda == 12 || sda == 16 || sda == 20 || sda == 0
                    || sda == 4 || sda == 8) ? 0 : 1,
        .sda_pin = sda,
        .scl_pin = scl,
        .freq_hz = WII_I2C_FREQ_HZ,
    };
    bus = platform_i2c_init(&cfg);
    if (!bus) {
        printf("[wii_host] ERROR: platform_i2c_init failed (SDA=%d SCL=%d)\n",
               sda, scl);
        return;
    }

    ext_transport.write    = io_write;
    ext_transport.read     = io_read;
    ext_transport.delay_us = io_delay;
    ext_transport.ctx      = bus;
    wii_ext_attach(&ext, &ext_transport);

    initialized = true;
    last_poll_us = 0;
    last_retry_us = 0;
    prev_connected = false;
    prev_buttons = 0;
    prev_analog = 0;

    printf("[wii_host] ready SDA=%d SCL=%d @ %uHz (bus %d)\n",
           sda, scl, (unsigned)WII_I2C_FREQ_HZ, cfg.bus);
}

void wii_host_task(void) {
    if (!initialized) return;

    uint32_t now = time_us_32();

    if (!ext.ready) {
        if ((now - last_retry_us) < WII_RETRY_INTERVAL_US && last_retry_us != 0) {
            return;
        }
        last_retry_us = now;

        // LED scan-blink: alternate dim white / off each retry tick.
        led_scan_state = !led_scan_state;
        leds_set_color(led_scan_state ? 8 : 0,
                       led_scan_state ? 8 : 0,
                       led_scan_state ? 8 : 0);

        if (wii_ext_start(&ext)) {
            printf("[wii_host] detected type=%d id=%02X:%02X:%02X:%02X:%02X:%02X\n",
                   (int)ext.type,
                   ext.id[0], ext.id[1], ext.id[2],
                   ext.id[3], ext.id[4], ext.id[5]);
        }
        return;
    }

    if ((now - last_poll_us) < WII_POLL_INTERVAL_US) return;
    last_poll_us = now;

    wii_ext_state_t state;
    if (!wii_ext_poll(&ext, &state)) {
        if (prev_connected) {
            printf("[wii_host] disconnected\n");
            prev_connected = false;
            leds_set_color(0, 0, 0);  // LED off on disconnect
        }
        return;
    }
    if (!prev_connected) {
        printf("[wii_host] connected type=%d\n", (int)state.type);
        prev_connected = true;
        prev_buttons = 0xFFFFFFFFu;
        prev_analog  = 0xFFFFFFFFFFFFFFFFull;
        // Status LED: solid per-accessory color on (re)connect.
        if (state.type == WII_EXT_TYPE_NUNCHUCK) {
            leds_set_color(LED_WII_NUNCHUCK_R, LED_WII_NUNCHUCK_G, LED_WII_NUNCHUCK_B);
        } else {
            leds_set_color(LED_WII_CLASSIC_R, LED_WII_CLASSIC_G, LED_WII_CLASSIC_B);
        }
    }

    // ------------------------------------------------------------------
    // Profile-cycle hotkey: MINUS (S1) + D-pad Up/Down held ≥ 2 s.
    // Only meaningful on accessories with a MINUS + D-pad (Classic/Pro).
    // Nunchuck has neither so this block no-ops for it.
    // ------------------------------------------------------------------
    {
        const uint32_t trigger_up   = WII_BTN_MINUS | WII_BTN_DU;
        const uint32_t trigger_down = WII_BTN_MINUS | WII_BTN_DD;
        uint32_t held = 0;
        if ((state.buttons & trigger_up)   == trigger_up)   held = trigger_up;
        if ((state.buttons & trigger_down) == trigger_down) held = trigger_down;

        if (held) {
            if (hotkey_combo_mask != held) {
                hotkey_combo_mask     = held;
                hotkey_combo_start_us = now;
                hotkey_fired          = false;
            } else if (!hotkey_fired &&
                       (now - hotkey_combo_start_us) >= WII_HOTKEY_HOLD_US) {
                // Resolve which output the app routes to, so this works
                // for wii2usb, wii2gc, wii2pce, etc. without per-app glue.
                output_target_t primary = router_get_primary_output();
                if (primary == OUTPUT_TARGET_NONE) primary = OUTPUT_TARGET_USB_DEVICE;
                if (held == trigger_up) {
                    profile_cycle_next(primary);
                    printf("[wii_host] profile: next (output=%d)\n", (int)primary);
                } else {
                    profile_cycle_prev(primary);
                    printf("[wii_host] profile: prev (output=%d)\n", (int)primary);
                }
                leds_indicate_profile(profile_get_active_index(primary));
                hotkey_fired = true;
            }
        } else {
            hotkey_combo_mask     = 0;
            hotkey_combo_start_us = 0;
            hotkey_fired          = false;
        }
    }

    input_event_t ev;
    init_input_event(&ev);
    ev.dev_addr  = WII_DEV_ADDR_BASE;
    ev.instance  = 0;
    ev.type      = INPUT_TYPE_GAMEPAD;
    ev.transport = INPUT_TRANSPORT_NATIVE;

    switch (state.type) {
        case WII_EXT_TYPE_NUNCHUCK:    map_nunchuck(&state, &ev);   break;
        case WII_EXT_TYPE_CLASSIC:
        case WII_EXT_TYPE_CLASSIC_PRO: map_classic(&state, &ev);    break;
        case WII_EXT_TYPE_GUITAR:      map_guitar(&state, &ev);     break;
        case WII_EXT_TYPE_DRUMS:       map_drums(&state, &ev);      break;
        case WII_EXT_TYPE_TURNTABLE:   map_turntable(&state, &ev);  break;
        case WII_EXT_TYPE_TAIKO:       map_taiko(&state, &ev);      break;
        case WII_EXT_TYPE_UDRAW:       map_udraw(&state, &ev);      break;
        case WII_EXT_TYPE_MOTIONPLUS:  map_motionplus(&state, &ev); break;
        default: return;
    }

    uint64_t analog_sig =
          ((uint64_t)ev.analog[ANALOG_LX] <<  0)
        | ((uint64_t)ev.analog[ANALOG_LY] <<  8)
        | ((uint64_t)ev.analog[ANALOG_RX] << 16)
        | ((uint64_t)ev.analog[ANALOG_RY] << 24)
        | ((uint64_t)ev.analog[ANALOG_L2] << 32)
        | ((uint64_t)ev.analog[ANALOG_R2] << 40);
    if (ev.buttons == prev_buttons && analog_sig == prev_analog) return;
    prev_buttons = ev.buttons;
    prev_analog  = analog_sig;

    router_submit_input(&ev);
}

bool wii_host_is_connected(void) {
    return initialized && ext.ready;
}

int wii_host_get_ext_type(void) {
    return (int)ext.type;
}

// ---- InputInterface ---------------------------------------------------------

static uint8_t wii_get_device_count(void) {
    return wii_host_is_connected() ? 1 : 0;
}

const InputInterface wii_input_interface = {
    .name             = "Wii",
    .source           = INPUT_SOURCE_NATIVE_WII,
    .init             = wii_host_init,
    .task             = wii_host_task,
    .is_connected     = wii_host_is_connected,
    .get_device_count = wii_get_device_count,
};
