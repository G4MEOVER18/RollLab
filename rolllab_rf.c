// rolllab_rf.c — CC1101 layer via subghz_devices API (no jamming — RX + TX only)
#include "rolllab_rf.h"
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <lib/toolbox/level_duration.h>

// Interrupt-accessible globals
static volatile RollLabApp* s_ref_app  = NULL;
static volatile RollLabApp* s_work_app = NULL;
static volatile RollLabApp* s_replay_app = NULL;

// ---------------------------------------------------------------------------
// RX CALLBACK — reference buffer
// ---------------------------------------------------------------------------
static void rlab_rx_ref_cb(bool level, uint32_t duration, void* ctx) {
    UNUSED(ctx);
    RollLabApp* app = (RollLabApp*)s_ref_app;
    if(!app) return;
    RLabRefSignal* sig = &app->ref_sig;
    if(sig->ready || duration == 0) return;

    if(sig->count < RLAB_REF_BUF_SIZE) {
        uint32_t d = (duration > 0x7FFFFFFFUL) ? 0x7FFFFFFFUL : duration;
        sig->buf[sig->count++] = (level ? 0x80000000UL : 0UL) | d;
    }

    if((!level && duration >= RLAB_SILENCE_US && sig->count >= RLAB_MIN_EDGES) ||
       sig->count >= RLAB_REF_BUF_SIZE) {
        sig->ready = true;
    }
}

// ---------------------------------------------------------------------------
// RX CALLBACK — working buffer (press detection)
// ---------------------------------------------------------------------------
static void rlab_rx_work_cb(bool level, uint32_t duration, void* ctx) {
    UNUSED(ctx);
    RollLabApp* app = (RollLabApp*)s_work_app;
    if(!app) return;
    RLabWorkSignal* sig = &app->work_sig;
    if(sig->ready || duration == 0) return;

    if(sig->count < RLAB_WORK_BUF_SIZE) {
        uint32_t d = (duration > 0x7FFFFFFFUL) ? 0x7FFFFFFFUL : duration;
        sig->buf[sig->count++] = (level ? 0x80000000UL : 0UL) | d;
    }

    if((!level && duration >= RLAB_SILENCE_US && sig->count >= RLAB_MIN_EDGES) ||
       sig->count >= RLAB_WORK_BUF_SIZE) {
        sig->ready = true;
    }
}

// ---------------------------------------------------------------------------
// TX CALLBACK — replay reference signal
// ---------------------------------------------------------------------------
static LevelDuration rlab_replay_tx_cb(void* ctx) {
    UNUSED(ctx);
    RollLabApp* app = (RollLabApp*)s_replay_app;
    if(!app) return level_duration_reset();

    const RLabRefSignal* sig = &app->ref_sig;
    size_t pos = app->tx_pos;
    if(pos >= sig->count) return level_duration_reset();

    app->tx_pos = pos + 1;
    uint32_t packed = sig->buf[pos];
    bool lv      = (packed & 0x80000000UL) != 0;
    uint32_t dur = packed & 0x7FFFFFFFUL;
    return level_duration_make(lv, dur > 0 ? dur : 100);
}

// ---------------------------------------------------------------------------
// PUBLIC API
// ---------------------------------------------------------------------------

bool rlab_rf_init(RollLabApp* app) {
    subghz_devices_init();
    // Prefer external CC1101 (GPIO), fall back to internal
    app->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
    if(!app->device) app->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!app->device) return false;

    subghz_devices_begin(app->device);
    subghz_devices_reset(app->device);
    subghz_devices_load_preset(app->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(app->device, app->frequency);
    subghz_devices_idle(app->device);
    app->rf_mode = RLAB_RF_IDLE;
    return true;
}

void rlab_rf_deinit(RollLabApp* app) {
    s_ref_app    = NULL;
    s_work_app   = NULL;
    s_replay_app = NULL;
    if(app->device) {
        subghz_devices_stop_async_tx(app->device);
        subghz_devices_stop_async_rx(app->device);
        subghz_devices_sleep(app->device);
        subghz_devices_end(app->device);
        app->device = NULL;
    }
    subghz_devices_deinit();
    app->rf_mode = RLAB_RF_IDLE;
}

void rlab_capture_ref_start(RollLabApp* app) {
    app->ref_sig.count = 0;
    app->ref_sig.ready = false;
    s_ref_app = app;
    subghz_devices_idle(app->device);
    subghz_devices_start_async_rx(app->device, rlab_rx_ref_cb, NULL);
    app->rf_mode = RLAB_RF_CAPTURING;
}

void rlab_capture_ref_stop(RollLabApp* app) {
    subghz_devices_stop_async_rx(app->device);
    s_ref_app = NULL;
    subghz_devices_idle(app->device);
    app->rf_mode = RLAB_RF_IDLE;
}

bool rlab_capture_ref_done(const RollLabApp* app) {
    return app->ref_sig.ready && app->ref_sig.count >= RLAB_MIN_EDGES;
}

void rlab_capture_work_start(RollLabApp* app) {
    app->work_sig.count = 0;
    app->work_sig.ready = false;
    s_work_app = app;
    subghz_devices_idle(app->device);
    subghz_devices_start_async_rx(app->device, rlab_rx_work_cb, NULL);
    app->rf_mode = RLAB_RF_CAPTURING;
}

void rlab_capture_work_stop(RollLabApp* app) {
    subghz_devices_stop_async_rx(app->device);
    s_work_app = NULL;
    subghz_devices_idle(app->device);
    app->rf_mode = RLAB_RF_IDLE;
}

bool rlab_capture_work_done(const RollLabApp* app) {
    return app->work_sig.ready && app->work_sig.count >= RLAB_MIN_EDGES;
}

void rlab_replay_start(RollLabApp* app) {
    app->tx_pos  = 0;
    s_replay_app = app;
    subghz_devices_idle(app->device);
    subghz_devices_start_async_tx(app->device, rlab_replay_tx_cb, NULL);
    app->rf_mode = RLAB_RF_REPLAYING;
}

void rlab_replay_stop(RollLabApp* app) {
    s_replay_app = NULL;
    subghz_devices_stop_async_tx(app->device);
    subghz_devices_idle(app->device);
    app->rf_mode = RLAB_RF_IDLE;
}

bool rlab_replay_done(const RollLabApp* app) {
    return (app->tx_pos >= app->ref_sig.count) ||
           subghz_devices_is_async_complete_tx(app->device);
}

void rlab_capture_stop_any(RollLabApp* app) {
    subghz_devices_stop_async_rx(app->device);
    s_ref_app  = NULL;
    s_work_app = NULL;
    subghz_devices_idle(app->device);
    app->rf_mode = RLAB_RF_IDLE;
}
