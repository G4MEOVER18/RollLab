#pragma once

#include "rolllab_app.h"

bool rlab_rf_init(RollLabApp* app);
void rlab_rf_deinit(RollLabApp* app);

// Capture into the reference signal buffer
void rlab_capture_ref_start(RollLabApp* app);
void rlab_capture_ref_stop(RollLabApp* app);
bool rlab_capture_ref_done(const RollLabApp* app);

// Capture into the working buffer (for press counting)
void rlab_capture_work_start(RollLabApp* app);
void rlab_capture_work_stop(RollLabApp* app);
bool rlab_capture_work_done(const RollLabApp* app);

// Stop any active capture (safe regardless of which type is active)
void rlab_capture_stop_any(RollLabApp* app);

// Replay the stored reference signal
void rlab_replay_start(RollLabApp* app);
void rlab_replay_stop(RollLabApp* app);
bool rlab_replay_done(const RollLabApp* app);
