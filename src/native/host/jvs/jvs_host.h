// jvs_host.h - Native JVS Controller Host Driver
//
// Polls native JVS controllers and submits input events to the router.
// Supports JVS 2L12B
#ifndef JVS_HOST_H
#define JVS_HOST_H

#include <stdint.h>
#include <stdbool.h>

#include "core/input_interface.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

#define JVS_UART              uart1
#define PIN_JVS_TX            4
#define PIN_JVS_RX            5
#define PIN_JVS_DE            2
#define PIN_JVS_RE            3
#define PIN_JVS_SENSE_IN_HIGH 7
#define PIN_JVS_SENSE_IN_LOW  6

#define JVS_MAX_PLAYERS       2

// Device types
#define JVSPAD_NONE           -1
#define JVSPAD_ARCADE_STICK   0

// ============================================================================
// PUBLIC API
// ============================================================================

// Initialize Arcade host driver
// Sets up GPIO pins
void jvs_host_init(void);

// Poll Arcade controllers and submit events to router
// Call this regularly from main loop (typically from app's task function)
void jvs_host_task(void);

// Get detected device type for a port
// Returns: -1=none, 0=ARCADE controller
int8_t jvs_host_get_device_type(uint8_t port);

// Check if any Arcade controller is connected
bool jvs_host_is_connected(void);

// ============================================================================
// INPUT INTERFACE
// ============================================================================

// Arcade input interface (implements InputInterface pattern for app declaration)
extern const InputInterface jvs_input_interface;

#endif // JVS_HOST_H
