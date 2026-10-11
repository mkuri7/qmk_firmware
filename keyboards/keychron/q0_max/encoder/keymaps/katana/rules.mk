# ============================================================
# Katana Q0 Firmware
# Keychron Q0 Max Encoder / ANSI
#
# Layer 0 : Mac Productivity
# Layer 1 : Katana Trading Control Deck
#
# This file also serves as a design record for optional
# QMK features considered for this firmware.
# ============================================================

VIA_ENABLE = yes
RAW_ENABLE = yes


# ------------------------------------------------------------
# Enabled in Katana Firmware v0.1
# ------------------------------------------------------------

# Used by:
#   0 + Enter -> Raw HID FLATTEN
#   . + Enter -> Raw HID REVERSE
COMBO_ENABLE = yes

# Useful for general Mac / development use.
CAPS_WORD_ENABLE = yes

# Reserved for future productivity workflows.
REPEAT_KEY_ENABLE = yes


# ------------------------------------------------------------
# Future candidates
# ------------------------------------------------------------

# TAP_DANCE_ENABLE = yes
#
# Enable when Katana actually defines tap_dance_actions[].
# Do not enable merely as a placeholder because QMK expects
# an actual Tap Dance definition when this feature is active.


# KEY_OVERRIDE_ENABLE = yes
#
# Enable when Katana actually defines key_overrides[].
# Possible future uses:
# - modifier-based Mac productivity behavior
# - DOM controls
# - alternate encoder behavior


# AUTO_SHIFT_ENABLE = yes
#
# Intentionally disabled.
# Katana prioritizes deterministic physical actions and avoids
# unnecessary time-based interpretation.


# LEADER_ENABLE = yes
#
# Potential future sequential-command mechanism.
# Protected trading actions currently use explicit combos.


# DYNAMIC_MACRO_ENABLE = yes
#
# Potential future Mac productivity feature.


# MOUSEKEY_ENABLE = yes
#
# Potential future Mac productivity feature.


# ------------------------------------------------------------
# Existing keyboard-level features
# ------------------------------------------------------------

# Encoder / RGB Matrix / Wireless support is provided by the
# Q0 Max keyboard definition.
