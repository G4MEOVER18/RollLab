#pragma once

typedef enum {
    RLAB_MENU,

    // Shared capture phase (mode determines next state)
    RLAB_CAPTURING,

    // Mode: Analyze
    RLAB_ANALYZE_VIEW,

    // Mode: Replay
    RLAB_REPLAY_READY,
    RLAB_REPLAYING,

    // Mode: Rollback / Sync Window
    RLAB_ADVANCE,       // Count receiver-advance presses via live RX
    RLAB_PROBE_READY,   // Ready to replay the old reference code
    RLAB_PROBING,       // TX old code in progress
    RLAB_RESULT,        // Display result / vulnerability verdict
} RollLabState;

typedef enum {
    RLAB_MODE_ANALYZE,
    RLAB_MODE_REPLAY,
    RLAB_MODE_ROLLBACK,
    RLAB_MODE_SYNCWIN,
} RollLabMode;

typedef enum {
    RLAB_RF_IDLE,
    RLAB_RF_CAPTURING,
    RLAB_RF_REPLAYING,
} RollLabRfMode;
