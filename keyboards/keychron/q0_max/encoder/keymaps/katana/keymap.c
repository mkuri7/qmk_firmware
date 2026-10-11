/* Copyright 2024 ~ 2026 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Katana Q0 Firmware v0.1.0
 *
 * Layer 0 : Mac Productivity
 * Layer 1 : Katana Trading Control Deck
 * Layer 2 : Keychron Utility
 *
 * Design principle:
 *
 *   Q0 sends physical intent.
 *   Katana owns trading semantics and safety.
 */

#include QMK_KEYBOARD_H
#include "keychron_common.h"
#include "katana_protocol.h"
#include "usb_device_state.h"


// ============================================================
// Layers
// ============================================================

enum layers {
    BASE,       // Layer 0 - Mac Productivity
    KATANA,     // Layer 1 - Katana Trading
    UTIL,       // Layer 2 - Keychron Utility
    L3,         // Reserved
};


// ============================================================
// Custom Keycodes
// ============================================================

enum custom_keycodes {
    K_LAYER = SAFE_RANGE,
    K_WINDOW_CAPTURE,
    K_LOCK_SCREEN,

    K_ARM_TOGGLE,
    K_OPEN_LONG,
    K_OPEN_SHORT,
    K_CANCEL_ALL,
    K_MOVE_TO_BE,
    K_EXIT_9,
    K_EXIT_6,
    K_EXIT_3,
    K_EXIT_1,

    // Katana combo-only physical keys
    K_FLAT,
    K_REVERSE,
    K_EXEC,
};

static bool    katana_host_ready = false;
static uint8_t katana_sequence   = 0;

typedef enum {
    KATANA_TRANSIENT_NONE,
    KATANA_TRANSIENT_ACCEPTED,
    KATANA_TRANSIENT_REJECTED,
} katana_transient_t;

typedef struct {
    bool               ready;
    bool               armed;
    bool               on_position;
    uint32_t           last_heartbeat_ms;
    katana_transient_t transient;
    uint32_t           transient_started_ms;
} KatanaDeckState;

static KatanaDeckState katana_deck_state = {0};

#define KATANA_HEARTBEAT_TIMEOUT_MS 4000
#define KATANA_TRANSIENT_MS 350

#define KATANA_NOT_READY_HUE 18
#define KATANA_NOT_READY_SAT 210
#define KATANA_NOT_READY_VAL 55
#define KATANA_READY_HUE 170
#define KATANA_READY_SAT 255
#define KATANA_READY_VAL 140
#define KATANA_ARMED_HUE 85
#define KATANA_ARMED_SAT 255
#define KATANA_ARMED_VAL 150
#define KATANA_POSITION_HUE 0
#define KATANA_POSITION_SAT 255
#define KATANA_POSITION_VAL 170
#define KATANA_POSITION_SPEED 32
#define KATANA_ACCEPTED_HUE 0
#define KATANA_ACCEPTED_SAT 0
#define KATANA_ACCEPTED_VAL 180
#define KATANA_REJECTED_HUE 12
#define KATANA_REJECTED_SAT 255
#define KATANA_REJECTED_VAL 180
#define KATANA_FIXED_SPEED 0

// QMK reloads persisted HSV/speed during the first frame after a mode change.
// Re-apply only HSV/speed after that frame without restarting the effect.
#define KATANA_RGB_REAPPLY_DELAY_MS 100

typedef struct {
    bool     pending;
    uint32_t started_ms;
    uint16_t hue;
    uint8_t  saturation;
    uint8_t  value;
    uint8_t  speed;
} katana_rgb_reapply_t;

static katana_rgb_reapply_t katana_rgb_reapply = {0};

static void katana_set_rgb_visual(uint8_t mode, uint16_t hue, uint8_t saturation, uint8_t value, uint8_t speed) {
#ifdef RGB_MATRIX_ENABLE
    rgb_matrix_mode_noeeprom(mode);
    rgb_matrix_sethsv_noeeprom(hue, saturation, value);
    rgb_matrix_set_speed_noeeprom(speed);

    katana_rgb_reapply.pending = true;
    katana_rgb_reapply.started_ms = timer_read32();
    katana_rgb_reapply.hue = hue;
    katana_rgb_reapply.saturation = saturation;
    katana_rgb_reapply.value = value;
    katana_rgb_reapply.speed = speed;
#else
    (void)mode;
    (void)hue;
    (void)saturation;
    (void)value;
    (void)speed;
#endif
}

static void katana_reapply_rgb_if_due(void) {
#ifdef RGB_MATRIX_ENABLE
    if (!katana_rgb_reapply.pending || timer_elapsed32(katana_rgb_reapply.started_ms) < KATANA_RGB_REAPPLY_DELAY_MS) {
        return;
    }

    // Do not set the mode again here: doing so would restart first-frame
    // initialization and cause another persisted-config reload.
    rgb_matrix_sethsv_noeeprom(katana_rgb_reapply.hue, katana_rgb_reapply.saturation, katana_rgb_reapply.value);
    rgb_matrix_set_speed_noeeprom(katana_rgb_reapply.speed);
    katana_rgb_reapply.pending = false;
#endif
}

static void katana_render_state(layer_state_t state) {
#ifdef RGB_MATRIX_ENABLE
    const bool katana_active = (state & ((layer_state_t)1 << KATANA)) != 0;
    if (!katana_active) {
        katana_rgb_reapply.pending = false;
        rgb_matrix_disable_noeeprom();
        return;
    }

    rgb_matrix_enable_noeeprom();
    if (katana_deck_state.transient == KATANA_TRANSIENT_ACCEPTED) {
        katana_set_rgb_visual(RGB_MATRIX_SOLID_COLOR, KATANA_ACCEPTED_HUE, KATANA_ACCEPTED_SAT, KATANA_ACCEPTED_VAL, KATANA_FIXED_SPEED);
    } else if (katana_deck_state.transient == KATANA_TRANSIENT_REJECTED) {
        katana_set_rgb_visual(RGB_MATRIX_SOLID_COLOR, KATANA_REJECTED_HUE, KATANA_REJECTED_SAT, KATANA_REJECTED_VAL, KATANA_FIXED_SPEED);
    } else if (katana_deck_state.on_position) {
        katana_set_rgb_visual(RGB_MATRIX_BAND_SPIRAL_VAL, KATANA_POSITION_HUE, KATANA_POSITION_SAT, KATANA_POSITION_VAL, KATANA_POSITION_SPEED);
    } else if (katana_deck_state.armed) {
        katana_set_rgb_visual(RGB_MATRIX_SOLID_COLOR, KATANA_ARMED_HUE, KATANA_ARMED_SAT, KATANA_ARMED_VAL, KATANA_FIXED_SPEED);
    } else if (katana_deck_state.ready) {
        katana_set_rgb_visual(RGB_MATRIX_SOLID_COLOR, KATANA_READY_HUE, KATANA_READY_SAT, KATANA_READY_VAL, KATANA_FIXED_SPEED);
    } else {
        katana_set_rgb_visual(RGB_MATRIX_SOLID_COLOR, KATANA_NOT_READY_HUE, KATANA_NOT_READY_SAT, KATANA_NOT_READY_VAL, KATANA_FIXED_SPEED);
    }
#else
    (void)state;
#endif
}

static void katana_clear_host_state(void) {
    katana_host_ready = false;
    katana_deck_state.ready = false;
    katana_deck_state.armed = false;
    katana_deck_state.on_position = false;
    katana_deck_state.transient = KATANA_TRANSIENT_NONE;
}

static void katana_apply_host_state(uint8_t flags) {
    katana_deck_state.ready = (flags & KATANA_STATE_READY) != 0;
    katana_deck_state.armed = (flags & KATANA_STATE_ARMED) != 0;
    katana_deck_state.on_position = (flags & KATANA_STATE_ON_POSITION) != 0;
    katana_deck_state.last_heartbeat_ms = timer_read32();
    katana_render_state(layer_state);
}

static void katana_send_command(uint8_t command_id) {
    if (!katana_host_ready) {
        return;
    }

    uint8_t report[KATANA_REPORT_SIZE];
    katana_packet_init(report, KATANA_MSG_COMMAND, command_id, ++katana_sequence);
    raw_hid_send(report, sizeof(report));
}

// Start macOS area capture to the clipboard. Window capture reuses this helper
// and switches the capture UI to Window mode after a short fixed delay.
#define WINDOW_CAPTURE_MODE_DELAY_MS 100

static void macos_area_capture_to_clipboard(void) {
    tap_code16(LCTL(LGUI(LSFT(KC_4))));
}

static void macos_window_capture_to_clipboard(void) {
    macos_area_capture_to_clipboard();
    wait_ms(WINDOW_CAPTURE_MODE_DELAY_MS);
    tap_code(KC_SPC);
}

bool keychron_raw_hid_receive_user(uint8_t src, uint8_t *data, uint8_t length) {
    (void)src;
    if (!katana_packet_has_magic(data, length)) {
        return false;
    }

    uint8_t response[KATANA_REPORT_SIZE];
    if (data[KATANA_OFFSET_VERSION] != KATANA_PROTOCOL_VERSION) {
        katana_packet_init(response, KATANA_MSG_NACK, KATANA_CMD_NONE, data[KATANA_OFFSET_SEQUENCE]);
        response[KATANA_OFFSET_PAYLOAD] = KATANA_PROTOCOL_VERSION;
        raw_hid_send(response, sizeof(response));
        katana_clear_host_state();
        katana_render_state(layer_state);
        return true;
    }

    if (data[KATANA_OFFSET_MESSAGE_TYPE] == KATANA_MSG_HELLO) {
        katana_packet_init(response, KATANA_MSG_HELLO_ACK, KATANA_CMD_NONE, data[KATANA_OFFSET_SEQUENCE]);
        response[KATANA_OFFSET_PAYLOAD + KATANA_HELLO_ACK_FW_MAJOR] = KATANA_FW_VERSION_MAJOR;
        response[KATANA_OFFSET_PAYLOAD + KATANA_HELLO_ACK_FW_MINOR] = KATANA_FW_VERSION_MINOR;
        response[KATANA_OFFSET_PAYLOAD + KATANA_HELLO_ACK_FW_PATCH] = KATANA_FW_VERSION_PATCH;
        response[KATANA_OFFSET_PAYLOAD + KATANA_HELLO_ACK_DEVICE_TYPE] = 0x01; /* Keychron Q0 Max */
        response[KATANA_OFFSET_PAYLOAD + KATANA_HELLO_ACK_CAPABILITIES] = KATANA_CAPABILITIES;
        raw_hid_send(response, sizeof(response));
        katana_host_ready = true;
        katana_deck_state.last_heartbeat_ms = timer_read32();
        katana_render_state(layer_state);
        return true;
    }

    if (!katana_host_ready) {
        katana_packet_init(response, KATANA_MSG_NACK, KATANA_CMD_NONE, data[KATANA_OFFSET_SEQUENCE]);
        raw_hid_send(response, sizeof(response));
        return true;
    }

    if (data[KATANA_OFFSET_MESSAGE_TYPE] == KATANA_MSG_HOST_STATE) {
        katana_apply_host_state(data[KATANA_OFFSET_PAYLOAD]);
        return true;
    }

    if (data[KATANA_OFFSET_MESSAGE_TYPE] == KATANA_MSG_HEARTBEAT) {
        katana_deck_state.last_heartbeat_ms = timer_read32();
        return true;
    }

    if (data[KATANA_OFFSET_MESSAGE_TYPE] == KATANA_MSG_FEEDBACK_EVENT) {
        const uint8_t event = data[KATANA_OFFSET_PAYLOAD];
        if (event == KATANA_FEEDBACK_COMMAND_ACCEPTED || event == KATANA_FEEDBACK_COMMAND_REJECTED) {
            katana_deck_state.transient = event == KATANA_FEEDBACK_COMMAND_ACCEPTED ? KATANA_TRANSIENT_ACCEPTED : KATANA_TRANSIENT_REJECTED;
            katana_deck_state.transient_started_ms = timer_read32();
            katana_deck_state.last_heartbeat_ms = timer_read32();
            katana_render_state(layer_state);
            return true;
        }
    }

    if (data[KATANA_OFFSET_MESSAGE_TYPE] == KATANA_MSG_ACK || data[KATANA_OFFSET_MESSAGE_TYPE] == KATANA_MSG_NACK) {
        return true;
    }

    katana_packet_init(response, KATANA_MSG_NACK, KATANA_CMD_NONE, data[KATANA_OFFSET_SEQUENCE]);
    raw_hid_send(response, sizeof(response));
    return true;
}


// ============================================================
// Layer Switch Configuration
// ============================================================
//
// M5 short press : no action
// M5 hold        : BASE <-> KATANA
//

#define KATANA_LAYER_HOLD_MS 500

static bool     katana_layer_key_down = false;
static bool     katana_layer_switched = false;
static uint16_t katana_layer_timer    = 0;
static uint8_t  katana_layer_target   = BASE;

// M4 is deliberately hold-only so an incidental tap cannot lock the Mac.
#define LOCK_SCREEN_HOLD_MS 850

static bool     lock_screen_key_down = false;
static bool     lock_screen_fired    = false;
static uint16_t lock_screen_timer    = 0;

// Triangle is press/release discriminated: short press selects Window mode,
// while a hold starts ordinary area selection exactly once.
#define TRIANGLE_HOLD_MS 550

static bool     triangle_key_down = false;
static bool     triangle_hold_fired = false;
static uint16_t triangle_timer = 0;


// ============================================================
// Katana Combos
// ============================================================
//
// 0 + Enter -> Raw HID FLATTEN
// . + Enter -> Raw HID REVERSE
//
// The component keys never emit individual host commands.
// Katana Next performs semantic interpretation and safety validation.
//

const uint16_t PROGMEM flatten_combo[] = {
    K_FLAT,
    K_EXEC,
    COMBO_END
};

const uint16_t PROGMEM reverse_combo[] = {
    K_REVERSE,
    K_EXEC,
    COMBO_END
};

combo_t key_combos[] = {
    COMBO_ACTION(flatten_combo),
    COMBO_ACTION(reverse_combo),
};

void process_combo_event(uint16_t combo_index, bool pressed) {
    if (!pressed) {
        return;
    }

    switch (combo_index) {
        case 0:
            katana_send_command(KATANA_CMD_FLATTEN);
            break;
        case 1:
            katana_send_command(KATANA_CMD_REVERSE);
            break;
    }
}


// ============================================================
// Keymaps
// ============================================================

// clang-format off

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    // ========================================================
    // Layer 0 - Mac Productivity
    // ========================================================
    //
    // Knob
    //   Rotate -> Volume
    //   Press  -> Mute
    //
    // Top Row
    //
    //   ○  Copy
    //
    //   △  Short -> Window Capture to Clipboard
    //      Hold  -> Area Selection to Clipboard
    //
    //   □  Paste
    //
    //   ×  Utility Layer while held
    //
    //
    // Macro Row
    //
    //   M1  Spotlight
    //   M2  Mission Control
    //   M3  Undo
    //   M4  Hold -> Lock Screen
    //   M5  Hold -> Katana Layer
    //
    //
    // Keypad
    //
    //   Num Clear -> Escape
    //   /         -> Tab
    //   *         -> Delete
    //   -         -> -
    //
    //   +         -> Keypad +
    //   Enter     -> Keypad Enter
    //
    //   Numbers remain normal keypad numbers
    //

    [BASE] = LAYOUT_tenkey_27(
        KC_MUTE,          LGUI(KC_C),        K_WINDOW_CAPTURE,  LGUI(KC_V),              MO(UTIL),

        LGUI(KC_SPC),     KC_ESC,            KC_TAB,            KC_DEL,                   KC_PMNS,

        LCTL(KC_UP),      KC_P7,             KC_P8,             KC_P9,                    KC_PPLS,

        LGUI(KC_Z),       KC_P4,             KC_P5,             KC_P6,

        K_LOCK_SCREEN,    KC_P1,             KC_P2,             KC_P3,                    KC_PENT,

        K_LAYER,          KC_P0,                                  KC_PDOT
    ),

    // ========================================================
    // Layer 1 - Katana Trading Control Deck
    // ========================================================
    //
    // M1             ARM / DISARM (Raw HID)
    // M2             OPEN LONG (Raw HID)
    // M3             OPEN SHORT (Raw HID)
    // M4        Reserved
    // M5 hold        -> BASE
    //
    // Num Clear      CANCEL ALL (Raw HID)
    // -              MOVE TO BE (Raw HID)
    //
    // 9              EXIT 9 (Raw HID)
    // 6              EXIT 6 (Raw HID)
    // 3              EXIT 3 (Raw HID)
    // 1              EXIT 1 (Raw HID)
    //
    // 0 + Enter      FLATTEN (Raw HID combo)
    // . + Enter      REVERSE (Raw HID combo)
    //
    // × hold         -> Utility Layer
    //

    [KATANA] = LAYOUT_tenkey_27(
        KC_NO,            KC_NO,             KC_NO,             KC_NO,              MO(UTIL),

        K_ARM_TOGGLE,     K_CANCEL_ALL,      KC_NO,             KC_NO,              K_MOVE_TO_BE,

        K_OPEN_LONG,      KC_NO,             KC_NO,             K_EXIT_9,           KC_NO,

        K_OPEN_SHORT,     KC_NO,             KC_NO,             K_EXIT_6,

        KC_NO,            K_EXIT_1,          KC_NO,             K_EXIT_3,           K_EXEC,

        K_LAYER,          K_FLAT,                                  K_REVERSE
    ),


    // ========================================================
    // Layer 2 - Keychron Utility
    // ========================================================
    //
    // Enter this layer by holding ×.
    //
    // Knob Press -> QK_BOOT
    //               Enter STM32 DFU Bootloader
    //
    // This provides a software path into DFU so future firmware
    // updates do not require access to the physical RESET button.
    //
    // IMPORTANT:
    //   × must be held while the knob is pressed.
    //   QK_BOOT is intentionally protected from accidental
    //   activation during normal operation.
    //

    [UTIL] = LAYOUT_tenkey_27(
        QK_BOOT,          BT_HST1,           BT_HST2,           BT_HST3,            P2P4G,

        _______,          UG_NEXT,           UG_VALU,           UG_HUEU,            _______,

        _______,          UG_PREV,           UG_VALD,           UG_HUED,            _______,

        _______,          UG_SATU,           UG_SPDU,           KC_MPRV,

        _______,          UG_SATD,           UG_SPDD,           KC_MPLY,            _______,

        _______,          UG_TOGG,                                KC_MNXT
    ),


    // ========================================================
    // Layer 3 - Reserved
    // ========================================================

    [L3] = LAYOUT_tenkey_27(
        _______,          _______,           _______,           _______,            _______,

        _______,          _______,           _______,           _______,            _______,

        _______,          _______,           _______,           _______,            _______,

        _______,          _______,           _______,           _______,

        _______,          _______,           _______,           _______,            _______,

        _______,          _______,                                _______
    )
};

// clang-format on


// ============================================================
// Encoder
// ============================================================

#if defined(ENCODER_MAP_ENABLE)

const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {

    // Layer 0
    // Volume Down / Up
    [BASE]   = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},

    // Layer 1
    // Reserved for future Katana functionality
    [KATANA] = {ENCODER_CCW_CW(KC_NO, KC_NO)},

    // Layer 2
    // RGB brightness
    [UTIL]   = {ENCODER_CCW_CW(UG_VALD, UG_VALU)},

    // Reserved
    [L3]     = {ENCODER_CCW_CW(KC_NO, KC_NO)},
};

#endif // ENCODER_MAP_ENABLE


// ============================================================
// Key Processing
// ============================================================

bool process_record_user(uint16_t keycode, keyrecord_t *record) {

    switch (keycode) {

        case K_WINDOW_CAPTURE:
            if (record->event.pressed) {
                triangle_key_down = true;
                triangle_hold_fired = false;
                triangle_timer = timer_read();
            } else if (triangle_key_down) {
                if (!triangle_hold_fired) {
                    macos_window_capture_to_clipboard();
                }
                triangle_key_down = false;
                triangle_hold_fired = false;
            }
            return false;

        case K_LOCK_SCREEN:
            if (record->event.pressed) {
                lock_screen_key_down = true;
                lock_screen_fired    = false;
                lock_screen_timer    = timer_read();
            } else {
                lock_screen_key_down = false;
                lock_screen_fired    = false;
            }
            return false;

        // ====================================================
        // M5 Layer Switch
        // ====================================================
        //
        // Hold:
        //   BASE   -> KATANA
        //   KATANA -> BASE
        //
        // Short press:
        //   No action
        //

        case K_LAYER:

            if (record->event.pressed) {

                katana_layer_key_down = true;
                katana_layer_switched = false;
                katana_layer_timer    = timer_read();

                if (layer_state_is(KATANA)) {
                    katana_layer_target = BASE;
                } else {
                    katana_layer_target = KATANA;
                }

            } else {

                katana_layer_key_down = false;
                katana_layer_switched = false;
            }

            return false;

        case K_ARM_TOGGLE:
            if (record->event.pressed) katana_send_command(KATANA_CMD_ARM_TOGGLE);
            return false;
        case K_OPEN_LONG:
            if (record->event.pressed) katana_send_command(KATANA_CMD_OPEN_LONG);
            return false;
        case K_OPEN_SHORT:
            if (record->event.pressed) katana_send_command(KATANA_CMD_OPEN_SHORT);
            return false;
        case K_CANCEL_ALL:
            if (record->event.pressed) katana_send_command(KATANA_CMD_CANCEL_ALL);
            return false;
        case K_MOVE_TO_BE:
            if (record->event.pressed) katana_send_command(KATANA_CMD_MOVE_TO_BE);
            return false;
        case K_EXIT_9:
            if (record->event.pressed) katana_send_command(KATANA_CMD_EXIT_9);
            return false;
        case K_EXIT_6:
            if (record->event.pressed) katana_send_command(KATANA_CMD_EXIT_6);
            return false;
        case K_EXIT_3:
            if (record->event.pressed) katana_send_command(KATANA_CMD_EXIT_3);
            return false;
        case K_EXIT_1:
            if (record->event.pressed) katana_send_command(KATANA_CMD_EXIT_1);
            return false;


        // ====================================================
        // Katana Combo-only Keys
        // ====================================================
        //
        // These keys intentionally produce no individual
        // host input.
        //
        // 0     -> K_FLAT
        // .     -> K_REVERSE
        // Enter -> K_EXEC
        //
        // Their meaning only exists when used as a combo.
        //

        case K_FLAT:
        case K_REVERSE:
        case K_EXEC:
            return false;
    }

    return true;
}


// ============================================================
// Layer Switching
// ============================================================
//
// M5 must remain held for KATANA_LAYER_HOLD_MS.
//
// This is intentionally slower than ordinary key operation so
// accidental layer switching is unlikely.
//

void matrix_scan_user(void) {

    katana_reapply_rgb_if_due();

    if (triangle_key_down &&
        !triangle_hold_fired &&
        timer_elapsed(triangle_timer) >= TRIANGLE_HOLD_MS) {
        triangle_hold_fired = true;
        macos_area_capture_to_clipboard();
    }

    if (katana_deck_state.transient != KATANA_TRANSIENT_NONE &&
        timer_elapsed32(katana_deck_state.transient_started_ms) >= KATANA_TRANSIENT_MS) {
        katana_deck_state.transient = KATANA_TRANSIENT_NONE;
        katana_render_state(layer_state);
    }

    if (katana_host_ready &&
        timer_elapsed32(katana_deck_state.last_heartbeat_ms) >= KATANA_HEARTBEAT_TIMEOUT_MS) {
        katana_clear_host_state();
        katana_render_state(layer_state);
    }

    if (
        lock_screen_key_down &&
        !lock_screen_fired &&
        timer_elapsed(lock_screen_timer) >= LOCK_SCREEN_HOLD_MS
    ) {
        lock_screen_fired = true;
        tap_code16(LCTL(LGUI(KC_Q)));
    }

    if (
        katana_layer_key_down &&
        !katana_layer_switched &&
        timer_elapsed(katana_layer_timer) >= KATANA_LAYER_HOLD_MS
    ) {

        katana_layer_switched = true;

        layer_move(katana_layer_target);
    }
}


// ============================================================
// RGB Layer Indication
// ============================================================
//
// BASE:
//   RGB OFF
//
// KATANA:
//   RGB ON
//
// UTIL entered from KATANA:
//   RGB remains ON
//
// KATANA visuals are selected from reconciled host state. Runtime HSV/speed
// values are explicit and non-persistent so saved QMK settings cannot alter
// the safety indication after a mode change.
//
// _noeeprom() is intentional.
// Layer changes must not repeatedly write persistent storage.
//

layer_state_t layer_state_set_user(layer_state_t state) {
    if ((state & ((layer_state_t)1 << KATANA)) != 0) {
        triangle_key_down = false;
        triangle_hold_fired = false;
    }
    katana_render_state(state);
    return state;
}


// ============================================================
// Startup / USB Reconnect
// ============================================================
//
// A USB reconnect does not necessarily reboot the MCU, so layer_state and RGB
// can otherwise retain the previous KATANA runtime state. Normalize both the
// cold-start and newly configured USB paths without writing EEPROM.
//
// BASE + RGB OFF is the deterministic safe state. Entering KATANA later enables
// the saved RGB configuration through layer_state_set_user().
//

static void katana_reset_to_safe_base(void) {
    triangle_key_down = false;
    triangle_hold_fired = false;
    katana_rgb_reapply.pending = false;
    katana_clear_host_state();
    default_layer_set((layer_state_t)1 << BASE);
    layer_move(BASE);

#ifdef RGB_MATRIX_ENABLE
    rgb_matrix_disable_noeeprom();
#endif
}

void keyboard_post_init_user(void) {
    katana_reset_to_safe_base();
}

void notify_usb_device_state_change_user(struct usb_device_state usb_device_state) {
    static usb_configure_state_t previous_state = USB_DEVICE_STATE_NO_INIT;
    const bool newly_configured =
        usb_device_state.configure_state == USB_DEVICE_STATE_CONFIGURED &&
        previous_state != USB_DEVICE_STATE_CONFIGURED;

    previous_state = usb_device_state.configure_state;

    if (newly_configured) {
        katana_reset_to_safe_base();
    }
}
