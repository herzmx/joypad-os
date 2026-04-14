// joywing_input.c - Adafruit Joy FeatherWing Input Interface (multi-instance)
//
// Supports up to 2 JoyWing modules. Each has a 2-axis joystick + 5 buttons.
// JoyWing 0: Left stick, D-pad buttons, S1 (Select)
// JoyWing 1: Right stick, B1-B4 face buttons, S2 (Start)
// Both submit to the same merged input event via router.

#include "joywing_input.h"
#include "drivers/seesaw/seesaw.h"
#include "core/buttons.h"
#include "core/input_event.h"
#include "core/router/router.h"
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>

// Joy FeatherWing button pin assignments (seesaw GPIO numbers)
#define JOYWING_BTN_A      6
#define JOYWING_BTN_B      7
#define JOYWING_BTN_X      9
#define JOYWING_BTN_Y      10
#define JOYWING_BTN_SELECT 14

// Joystick ADC channels
#define JOYWING_ADC_X      1
#define JOYWING_ADC_Y      0

// Button pin mask for GPIO bulk read
#define JOYWING_BTN_MASK ((1u << JOYWING_BTN_A) | (1u << JOYWING_BTN_B) | \
                          (1u << JOYWING_BTN_X) | (1u << JOYWING_BTN_Y) | \
                          (1u << JOYWING_BTN_SELECT))

// Device addresses for router (0xE0+ range for I2C devices)
#define JOYWING_DEV_ADDR_BASE 0xE0

#define JOYWING_MAX_INSTANCES 2

// Per-instance state
typedef struct {
    joywing_config_t cfg;
    seesaw_device_t seesaw;
    platform_i2c_t i2c_bus;
    input_event_t event;
    bool configured;
    bool initialized;
    uint32_t last_poll;
} joywing_instance_t;

static joywing_instance_t instances[JOYWING_MAX_INSTANCES];
static uint8_t instance_count = 0;

void joywing_input_init_config(const joywing_config_t* config)
{
    if (instance_count >= JOYWING_MAX_INSTANCES) {
        printf("[joywing] Max instances (%d) reached\n", JOYWING_MAX_INSTANCES);
        return;
    }
    instances[instance_count].cfg = *config;
    instances[instance_count].configured = true;
    instance_count++;
}

static void joywing_init_instance(uint8_t idx)
{
    joywing_instance_t* jw = &instances[idx];
    if (!jw->configured) return;

    // Initialize I2C bus
    platform_i2c_config_t i2c_cfg = {
        .bus = jw->cfg.i2c_bus,
        .sda_pin = jw->cfg.sda_pin,
        .scl_pin = jw->cfg.scl_pin,
        .freq_hz = 400000,
    };
    jw->i2c_bus = platform_i2c_init(&i2c_cfg);
    if (!jw->i2c_bus) {
        printf("[joywing:%d] I2C init failed\n", idx);
        return;
    }

    // Initialize seesaw
    uint8_t addr = jw->cfg.addr ? jw->cfg.addr : SEESAW_ADDR_DEFAULT;
    seesaw_init(&jw->seesaw, jw->i2c_bus, addr);

    uint8_t hw_id = seesaw_get_hw_id(&jw->seesaw);
    printf("[joywing:%d] Seesaw HW ID: 0x%02X (addr=0x%02X)\n", idx, hw_id, addr);

    // Configure button pins
    if (!seesaw_gpio_set_input_pullup(&jw->seesaw, JOYWING_BTN_MASK)) {
        printf("[joywing:%d] GPIO config failed\n", idx);
    }

    // Initialize input event
    init_input_event(&jw->event);
    jw->event.dev_addr = JOYWING_DEV_ADDR_BASE + idx;
    jw->event.instance = idx;
    jw->event.type = INPUT_TYPE_GAMEPAD;
    jw->event.transport = INPUT_TRANSPORT_I2C;

    jw->initialized = true;
    jw->last_poll = 0;
    printf("[joywing:%d] Initialized\n", idx);
}

static void joywing_init(void)
{
    for (uint8_t i = 0; i < instance_count; i++) {
        joywing_init_instance(i);
    }
    printf("[joywing] %d instance(s) initialized\n", instance_count);
}

static void joywing_poll_instance(uint8_t idx)
{
    joywing_instance_t* jw = &instances[idx];
    if (!jw->initialized) return;

    // Rate-limit to ~100Hz per instance
    uint32_t now = platform_time_ms();
    if (now - jw->last_poll < 10) return;
    jw->last_poll = now;

    // Read buttons (active low)
    uint32_t gpio = seesaw_gpio_read_bulk(&jw->seesaw);
    if (gpio == 0xFFFFFFFF) return;

    uint32_t buttons = 0;

    if (idx == 0) {
        // JoyWing 0: joystick as D-pad + Select button as S1
        // Also map physical buttons as D-pad for digital input
        if (!(gpio & (1u << JOYWING_BTN_A)))      buttons |= JP_BUTTON_DR;   // A → D-Right
        if (!(gpio & (1u << JOYWING_BTN_B)))      buttons |= JP_BUTTON_DD;   // B → D-Down
        if (!(gpio & (1u << JOYWING_BTN_X)))      buttons |= JP_BUTTON_DU;   // X → D-Up
        if (!(gpio & (1u << JOYWING_BTN_Y)))      buttons |= JP_BUTTON_DL;   // Y → D-Left
        if (!(gpio & (1u << JOYWING_BTN_SELECT))) buttons |= JP_BUTTON_S1;   // Select → S1
    } else {
        // JoyWing 1: face buttons + Start
        if (!(gpio & (1u << JOYWING_BTN_A)))      buttons |= JP_BUTTON_B2;   // A → B2 (Circle)
        if (!(gpio & (1u << JOYWING_BTN_B)))      buttons |= JP_BUTTON_B1;   // B → B1 (Cross)
        if (!(gpio & (1u << JOYWING_BTN_X)))      buttons |= JP_BUTTON_B3;   // X → B3 (Square)
        if (!(gpio & (1u << JOYWING_BTN_Y)))      buttons |= JP_BUTTON_B4;   // Y → B4 (Triangle)
        if (!(gpio & (1u << JOYWING_BTN_SELECT))) buttons |= JP_BUTTON_S2;   // Select → S2
    }

    platform_sleep_us(500);

    // Read joystick
    uint16_t raw_x = seesaw_adc_read(&jw->seesaw, JOYWING_ADC_X);
    if (raw_x == SEESAW_ADC_ERROR) goto submit;

    platform_sleep_us(500);

    uint16_t raw_y = seesaw_adc_read(&jw->seesaw, JOYWING_ADC_Y);
    if (raw_y == SEESAW_ADC_ERROR) goto submit;

    if (idx == 0) {
        // JoyWing 0: left stick
        jw->event.analog[ANALOG_LX] = (uint8_t)(raw_x >> 2);
        jw->event.analog[ANALOG_LY] = (uint8_t)(raw_y >> 2);
    } else {
        // JoyWing 1: right stick
        jw->event.analog[ANALOG_RX] = (uint8_t)(raw_x >> 2);
        jw->event.analog[ANALOG_RY] = (uint8_t)(raw_y >> 2);
    }

submit:
    jw->event.buttons = buttons;
    router_submit_input(&jw->event);
}

static void joywing_task(void)
{
    for (uint8_t i = 0; i < instance_count; i++) {
        joywing_poll_instance(i);
    }
}

static bool joywing_is_connected(void)
{
    for (uint8_t i = 0; i < instance_count; i++) {
        if (instances[i].initialized) return true;
    }
    return false;
}

static uint8_t joywing_get_device_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < instance_count; i++) {
        if (instances[i].initialized) count++;
    }
    return count;
}

const InputInterface joywing_input_interface = {
    .name = "JoyWing",
    .source = INPUT_SOURCE_GPIO,
    .init = joywing_init,
    .task = joywing_task,
    .is_connected = joywing_is_connected,
    .get_device_count = joywing_get_device_count,
};
