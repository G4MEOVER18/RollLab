#pragma once

#include <furi.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/toolbox/level_duration.h>
#include "rolllab_states.h"

// Reference capture: larger buffer for quality signal storage
#define RLAB_REF_BUF_SIZE       1024U
// Working buffer: smaller, for press detection during advance phase
#define RLAB_WORK_BUF_SIZE       512U
#define RLAB_MIN_EDGES            16U
#define RLAB_SILENCE_US        25000U   // 25 ms silence = signal boundary
#define RLAB_CAP_TIMEOUT_MS     9000U   // 9 s per capture attempt
#define RLAB_FREQUENCY      433920000UL // 433.92 MHz default

typedef struct {
    volatile uint32_t buf[RLAB_REF_BUF_SIZE];
    volatile size_t   count;
    volatile bool     ready;
} RLabRefSignal;

typedef struct {
    volatile uint32_t buf[RLAB_WORK_BUF_SIZE];
    volatile size_t   count;
    volatile bool     ready;
} RLabWorkSignal;

// Signal analysis results
typedef struct {
    uint32_t edge_count;
    uint32_t min_us;
    uint32_t max_us;
    uint32_t avg_us;
    uint32_t total_ms;      // Total signal duration in ms
    bool     preamble_ok;   // First edge is HIGH and >> avg
    bool     looks_ook;     // Bi-modal edge distribution (PWM/Manchester OOK)
} RLabAnalysis;

typedef struct {
    RollLabState        state;
    RollLabMode         mode;
    uint32_t            frequency;
    const SubGhzDevice* device;

    RLabRefSignal       ref_sig;    // Reference signal (stored for probe/replay)
    RLabWorkSignal      work_sig;   // Temporary capture buffer (advance counting)
    RLabAnalysis        analysis;

    uint8_t             menu_idx;
    uint8_t             advance_count;   // Presses detected during advance phase
    uint8_t             advance_target;  // Guidance: expected presses for this mode

    volatile bool       abort;
    volatile size_t     tx_pos;
    RollLabRfMode       rf_mode;
    char                status[64];
} RollLabApp;
