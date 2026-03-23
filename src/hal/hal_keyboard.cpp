/*
 * ============================================================
 *   CoCo_ESP32 Beta-1 March 2026 - CoCo 2 Emulator for ESP32-S3
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32_CoCo2_XRoar_Port
 *   Based on XRoar by Ciaran Anscomb
 *   ESP32 Port of XRoar co-developed with Claude Code (Anthropic)
 *   MIT License
 * ============================================================
 *  File   : hal_keyboard.cpp
 *  Module : Keyboard HAL — USB HID host events mapped to CoCo 7×8 matrix
 * ============================================================
 */

/*
 * hal_keyboard.cpp - Keyboard input via USB HID Host + matrix injection
 *
 * CoCo keyboard matrix (verified from BASIC ROM KEYIN routine):
 * key_code = PA_row * 8 + PB_column
 *
 *         PB0    PB1    PB2    PB3    PB4    PB5    PB6    PB7
 * PA0:     @      A      B      C      D      E      F      G
 * PA1:     H      I      J      K      L      M      N      O
 * PA2:     P      Q      R      S      T      U      V      W
 * PA3:     X      Y      Z     UP    DOWN   LEFT  RIGHT   SPACE
 * PA4:     0      1      2      3      4      5      6      7
 * PA5:     8      9      :      ;      ,      -      .      /
 * PA6:   ENTER  CLEAR  BREAK  (n/a)  (n/a)  (n/a)  (n/a)  SHIFT
 *
 * key_matrix[col] bit[row] = 0 if pressed (active LOW).
 * col = PB (0-7), row = PA (0-6).
 *
 * Two input sources merged into the same matrix:
 *   1. USB HID keyboard (via usb_kbd_host on Core 0)
 *   2. Software injection (hal_keyboard_press/release for tests)
 */

#include "hal.h"
#include "../utils/debug.h"
#include "usb_kbd_host.h"
#include "../supervisor/supervisor.h"
#include "../supervisor/sv_disk.h"

// ── USB HID modifier bitmasks ───────────────────────────────────────────────
#define MOD_LSHIFT  0x02
#define MOD_RSHIFT  0x20
#define MOD_SHIFT   (MOD_LSHIFT | MOD_RSHIFT)

// ── CoCo SHIFT key position (PA6, PB7) ─────────────────────────────────────
#define COCO_SHIFT_ROW  6
#define COCO_SHIFT_COL  7

// ── Key-map entry: HID usage → CoCo matrix position ────────────────────────
struct KeyMap {
    uint8_t hid_usage;
    uint8_t req_modifier;   // 0 = any, else modifier must be held
    uint8_t col;            // PB (0-7)
    uint8_t row;            // PA (0-6)
    bool    needs_shift;    // Assert CoCo SHIFT
    bool    suppress_shift; // Suppress CoCo SHIFT (PC shift produced this)
};

// ── HID → CoCo matrix mapping (CORRECT layout from ROM) ────────────────────
static const KeyMap KEY_MAP[] = {
    // Letters A-Z: HID 0x04-0x1D
    // PA0: @(PB0) A(PB1) B(PB2) C(PB3) D(PB4) E(PB5) F(PB6) G(PB7)
    { 0x04, 0, 1, 0, false, false },  // A → PA0,PB1
    { 0x05, 0, 2, 0, false, false },  // B → PA0,PB2
    { 0x06, 0, 3, 0, false, false },  // C → PA0,PB3
    { 0x07, 0, 4, 0, false, false },  // D → PA0,PB4
    { 0x08, 0, 5, 0, false, false },  // E → PA0,PB5
    { 0x09, 0, 6, 0, false, false },  // F → PA0,PB6
    { 0x0A, 0, 7, 0, false, false },  // G → PA0,PB7
    // PA1: H(PB0) I(PB1) J(PB2) K(PB3) L(PB4) M(PB5) N(PB6) O(PB7)
    { 0x0B, 0, 0, 1, false, false },  // H → PA1,PB0
    { 0x0C, 0, 1, 1, false, false },  // I → PA1,PB1
    { 0x0D, 0, 2, 1, false, false },  // J → PA1,PB2
    { 0x0E, 0, 3, 1, false, false },  // K → PA1,PB3
    { 0x0F, 0, 4, 1, false, false },  // L → PA1,PB4
    { 0x10, 0, 5, 1, false, false },  // M → PA1,PB5
    { 0x11, 0, 6, 1, false, false },  // N → PA1,PB6
    { 0x12, 0, 7, 1, false, false },  // O → PA1,PB7
    // PA2: P(PB0) Q(PB1) R(PB2) S(PB3) T(PB4) U(PB5) V(PB6) W(PB7)
    { 0x13, 0, 0, 2, false, false },  // P → PA2,PB0
    { 0x14, 0, 1, 2, false, false },  // Q → PA2,PB1
    { 0x15, 0, 2, 2, false, false },  // R → PA2,PB2
    { 0x16, 0, 3, 2, false, false },  // S → PA2,PB3
    { 0x17, 0, 4, 2, false, false },  // T → PA2,PB4
    { 0x18, 0, 5, 2, false, false },  // U → PA2,PB5
    { 0x19, 0, 6, 2, false, false },  // V → PA2,PB6
    { 0x1A, 0, 7, 2, false, false },  // W → PA2,PB7
    // PA3: X(PB0) Y(PB1) Z(PB2) UP(PB3) DOWN(PB4) LEFT(PB5) RIGHT(PB6) SPACE(PB7)
    { 0x1B, 0, 0, 3, false, false },  // X → PA3,PB0
    { 0x1C, 0, 1, 3, false, false },  // Y → PA3,PB1
    { 0x1D, 0, 2, 3, false, false },  // Z → PA3,PB2
    { 0x52, 0, 3, 3, false, false },  // UP → PA3,PB3
    { 0x51, 0, 4, 3, false, false },  // DOWN → PA3,PB4
    { 0x50, 0, 5, 3, false, false },  // LEFT → PA3,PB5
    { 0x4F, 0, 6, 3, false, false },  // RIGHT → PA3,PB6
    { 0x2C, 0, 7, 3, false, false },  // SPACE → PA3,PB7
    // PA4: 0(PB0) 1(PB1) 2(PB2) 3(PB3) 4(PB4) 5(PB5) 6(PB6) 7(PB7)
    { 0x27, 0, 0, 4, false, false },  // 0 → PA4,PB0
    { 0x1E, 0, 1, 4, false, false },  // 1 → PA4,PB1
    { 0x1F, 0, 2, 4, false, false },  // 2 → PA4,PB2
    { 0x20, 0, 3, 4, false, false },  // 3 → PA4,PB3
    { 0x21, 0, 4, 4, false, false },  // 4 → PA4,PB4
    { 0x22, 0, 5, 4, false, false },  // 5 → PA4,PB5
    { 0x23, 0, 6, 4, false, false },  // 6 → PA4,PB6
    { 0x24, 0, 7, 4, false, false },  // 7 → PA4,PB7
    // PA5: 8(PB0) 9(PB1) :(PB2) ;(PB3) ,(PB4) -(PB5) .(PB6) /(PB7)
    { 0x25, 0, 0, 5, false, false },  // 8 → PA5,PB0
    { 0x26, 0, 1, 5, false, false },  // 9 → PA5,PB1
    { 0x33, 0, 3, 5, false, false },  // ; → PA5,PB3
    { 0x36, 0, 4, 5, false, false },  // , → PA5,PB4
    { 0x2D, 0, 5, 5, false, false },  // - → PA5,PB5
    { 0x37, 0, 6, 5, false, false },  // . → PA5,PB6
    { 0x38, 0, 7, 5, false, false },  // / → PA5,PB7
    // PA6: ENTER(PB0) CLEAR(PB1) BREAK(PB2) ... SHIFT(PB7)
    { 0x28, 0, 0, 6, false, false },  // ENTER → PA6,PB0
    { 0x2A, 0, 5, 3, false, false },  // BACKSPACE → LEFT ARROW PA3,PB5
    { 0x48, 0, 2, 6, false, false },  // PAUSE → BREAK PA6,PB2
    { 0x29, 0, 2, 6, false, false },  // ESC → BREAK PA6,PB2
    { 0x46, 0, 2, 6, false, false },  // PRINTSCREEN → BREAK PA6,PB2
    { 0x49, 0, 1, 6, false, false },  // INSERT → CLEAR PA6,PB1
    { 0x4C, 0, 1, 6, false, false },  // DELETE → CLEAR PA6,PB1

    // Shift-modified: PC Shift+2 → CoCo @ (PA0,PB0), suppress CoCo SHIFT
    { 0x1F, MOD_SHIFT, 0, 0, false, true  },  // @ → PA0,PB0
    // PC Shift+; → CoCo : (PA5,PB2), suppress CoCo SHIFT
    { 0x33, MOD_SHIFT, 2, 5, false, true  },  // : → PA5,PB2

    // Shift keys → CoCo SHIFT (PA6,PB7)
    { 0xE1, 0, COCO_SHIFT_COL, COCO_SHIFT_ROW, false, false },  // L-SHIFT
    { 0xE5, 0, COCO_SHIFT_COL, COCO_SHIFT_ROW, false, false },  // R-SHIFT
};

static const size_t KEY_MAP_COUNT = sizeof(KEY_MAP) / sizeof(KEY_MAP[0]);

// ── Matrix state ────────────────────────────────────────────────────────────
// Indexed by column (PB), bits = rows (PA). Active low.
static uint8_t key_matrix[8] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// ── USB HID key mapping helpers ─────────────────────────────────────────────

static const KeyMap* find_mapping(uint8_t usage, uint8_t modifiers) {
    const KeyMap* fallback = nullptr;
    for (size_t i = 0; i < KEY_MAP_COUNT; i++) {
        if (KEY_MAP[i].hid_usage != usage) continue;
        if (KEY_MAP[i].req_modifier != 0 &&
            (modifiers & KEY_MAP[i].req_modifier) == KEY_MAP[i].req_modifier) {
            return &KEY_MAP[i];
        }
        if (KEY_MAP[i].req_modifier == 0 && !fallback) {
            fallback = &KEY_MAP[i];
        }
    }
    return fallback;
}

static void set_key(uint8_t col, uint8_t row, bool pressed) {
    if (col >= 8 || row >= 8) return;
    if (pressed)
        key_matrix[col] &= ~(1 << row);
    else
        key_matrix[col] |= (1 << row);
}

static void apply_shift(bool active) {
    set_key(COCO_SHIFT_COL, COCO_SHIFT_ROW, active);
}

// ── Deferred key release ────────────────────────────────────────────────────
// When a key is pressed, we must keep it in the matrix for at least
// MIN_HOLD_FRAMES so the CoCo BASIC KEYIN routine can detect and debounce it.
// Without this, fast key taps (press+release arriving in the same frame)
// are invisible to the emulated CPU.
#define MIN_HOLD_FRAMES 4
struct DeferredRelease {
    uint8_t col;
    uint8_t row;
    uint8_t frames_left;  // 0 = slot free
};
#define MAX_DEFERRED 8
static DeferredRelease deferred_releases[MAX_DEFERRED];

static void defer_release(uint8_t col, uint8_t row) {
    // If there's already a deferred release for this key, refresh it
    for (int i = 0; i < MAX_DEFERRED; i++) {
        if (deferred_releases[i].frames_left > 0 &&
            deferred_releases[i].col == col && deferred_releases[i].row == row) {
            deferred_releases[i].frames_left = MIN_HOLD_FRAMES;
            return;
        }
    }
    // Find free slot
    for (int i = 0; i < MAX_DEFERRED; i++) {
        if (deferred_releases[i].frames_left == 0) {
            deferred_releases[i].col = col;
            deferred_releases[i].row = row;
            deferred_releases[i].frames_left = MIN_HOLD_FRAMES;
            return;
        }
    }
    // No free slot — release immediately (fallback)
    set_key(col, row, false);
}

// Called once per frame to tick down deferred releases
static void tick_deferred_releases() {
    for (int i = 0; i < MAX_DEFERRED; i++) {
        if (deferred_releases[i].frames_left > 0) {
            deferred_releases[i].frames_left--;
            if (deferred_releases[i].frames_left == 0) {
                set_key(deferred_releases[i].col, deferred_releases[i].row, false);
            }
        }
    }
}

// ── Machine pointer for hotkey actions ───────────────────────────────────────
static Machine* s_machine_ptr = nullptr;

void hal_keyboard_set_machine(Machine* m) {
    s_machine_ptr = m;
}

// ── HID key event callback (called from hid_host_process on Core 1) ────────

static void on_hid_key(uint8_t usage, uint8_t modifiers, bool pressed) {
    // === Layer 1: Supervisor hotkeys (always checked, press only) ===
    if (usage == 0x3A && pressed) {  // F1 — toggle supervisor
        supervisor_toggle();
        return;  // Consume — never reaches CoCo matrix
    }

    if (supervisor_is_active()) {
        // === Layer 2b: All input goes to supervisor ===
        if (usage != 0) {
            supervisor_on_key(usage, pressed);
        }
        return;  // CoCo matrix sees nothing
    }

    // === Layer 1b: Emulation hotkeys (only when supervisor inactive) ===
    if (pressed && usage != 0) {
        if (usage == 0x3B) {  // F2 — soft reset (flush dirty disks first)
            if (s_machine_ptr) {
                sv_disk_flush_all(&s_machine_ptr->fdc);
                machine_reset(s_machine_ptr);
            }
            return;
        }
        if (usage == 0x3C) {  // F3 — quick mount last disk
            if (s_machine_ptr) supervisor_quick_mount_last_disk(s_machine_ptr);
            return;
        }
        // F4 reserved for future supervisor debugger
        if (usage == 0x3E) {  // F5 — toggle FPS overlay
            extern void hal_video_toggle_fps_overlay(void);
            hal_video_toggle_fps_overlay();
            return;
        }
    }

    // === Layer 2a: Forward to CoCo keyboard matrix ===
    bool shift_held = (modifiers & MOD_SHIFT) != 0;

    // Modifier-only change
    if (usage == 0) {
        apply_shift(shift_held);
        return;
    }

    const KeyMap* k = find_mapping(usage, modifiers);
    if (!k) return;

    bool is_shift_key = (k->col == COCO_SHIFT_COL && k->row == COCO_SHIFT_ROW);

    if (is_shift_key) {
        if (pressed) {
            apply_shift(true);
        } else {
            // Defer shift release too
            defer_release(COCO_SHIFT_COL, COCO_SHIFT_ROW);
        }
    } else {
        if (pressed) {
            bool want_shift = (shift_held && !k->suppress_shift) || k->needs_shift;
            apply_shift(want_shift);
            set_key(k->col, k->row, true);
        } else {
            // Defer key release so CoCo CPU has time to scan it
            defer_release(k->col, k->row);
            // Also handle shift release
            if (!shift_held) {
                defer_release(COCO_SHIFT_COL, COCO_SHIFT_ROW);
            }
        }
    }
}

// ── Public API ──────────────────────────────────────────────────────────────

void hal_keyboard_init(void) {
    hal_keyboard_release_all();
    memset(deferred_releases, 0, sizeof(deferred_releases));
    hid_host_begin(on_hid_key);
    DEBUG_PRINT("  Keyboard: USB HID host + matrix injection ready");
}

void hal_keyboard_tick(void) {
    tick_deferred_releases();
}

uint8_t hal_keyboard_scan(uint8_t column) {
    if (column < 8) {
        return key_matrix[column];
    }
    return 0xFF;
}

// Injection API (used by integration tests)
void hal_keyboard_press(uint8_t row, uint8_t col) {
    if (row < 8 && col < 8) {
        key_matrix[col] &= ~(1 << row);
    }
}

void hal_keyboard_release(uint8_t row, uint8_t col) {
    if (row < 8 && col < 8) {
        key_matrix[col] |= (1 << row);
    }
}

void hal_keyboard_release_all(void) {
    for (int i = 0; i < 8; i++) {
        key_matrix[i] = 0xFF;
    }
}
