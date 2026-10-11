/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once
#define KATANA_FW_VERSION "0.1.0"
#define KATANA_FW_VERSION_MAJOR 0
#define KATANA_FW_VERSION_MINOR 1
#define KATANA_FW_VERSION_PATCH 0

// ============================================================
// Katana Q0 Firmware - Timing Configuration
// ============================================================
//
// Protected trading actions:
//
//   0 + Enter  -> Raw HID FLATTEN
//   . + Enter  -> Raw HID REVERSE
//
// These combos must be fast enough for emergency operation,
// while still requiring an intentional two-key chord.
// ============================================================

// Maximum interval between the two keys of a combo.
//
// Initial value:
//   120 ms
//
// Tune after physical testing if necessary:
//
//   80-100 ms  -> stricter / faster
//   120 ms      -> initial Katana setting
//   150-200 ms  -> easier, but more permissive
#define COMBO_TERM 120


// Measure COMBO_TERM from the first key press.
//
// This makes the timing requirement deterministic and reduces
// the chance that loosely sequential key presses are interpreted
// as a protected trading command.
#define COMBO_STRICT_TIMER
