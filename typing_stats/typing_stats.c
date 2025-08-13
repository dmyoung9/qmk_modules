#include "typing_stats.h"
#include "eeprom.h"
#include "eeconfig.h"
#include "timer.h"
#include <string.h>
#include <math.h>

#define TS_MAGIC 0x54535432u // "TST2" - incremented for new features
#define TS_VERSION 0x0004

static ts_blob_t     g_blob;
static bool          g_loaded             = false;
static bool          g_dirty              = false;
static uint32_t      g_last_flush         = 0;
static uint32_t      g_event_ctr          = 0;
static layer_state_t g_layer_state_cached = 0;

#if TS_ENABLE_LAYER_TIME
static uint32_t      g_layer_time_start   = 0;
static uint8_t       g_current_layer      = 0;
#endif

#if TS_ENABLE_BIGRAM_STATS
static uint16_t      g_last_pos_index     = 0xFFFF;
#endif

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len);
static void     ts_load(void);
static void     ts_save_if_needed(bool force);
static void     ts_update_wpm_ema(uint16_t wpm);

_Static_assert(EECONFIG_USER_DATA_SIZE >= sizeof(ts_blob_t), "EECONFIG_USER_DATA_SIZE too small for ts_blob_t");

// ---- util ----
uint16_t ts_pos_to_index(uint8_t row, uint8_t col) {
    return (uint16_t)row * MATRIX_COLS + col;
}

void ts_index_to_pos(uint16_t index, uint8_t *row_out, uint8_t *col_out) {
    if (row_out) *row_out = index / MATRIX_COLS;
    if (col_out) *col_out = index % MATRIX_COLS;
}

ts_hand_t ts_pos_to_hand(uint8_t row, uint8_t col) {
    // Simple heuristic: left half vs right half of matrix
    if (col < MATRIX_COLS / 2) return TS_HAND_LEFT;
    if (col >= (MATRIX_COLS + 1) / 2) return TS_HAND_RIGHT;
    return TS_HAND_UNKNOWN;
}

static inline void ts_bump_pos(uint8_t row, uint8_t col) {
    uint16_t i = ts_pos_to_index(row, col);
    if (i < (MATRIX_ROWS * MATRIX_COLS)) {
        if (g_blob.c.pos[i].presses != UINT16_MAX) {
            g_blob.c.pos[i].presses++;
        }

        // Hand balance tracking
        ts_hand_t hand = ts_pos_to_hand(row, col);
        if (hand == TS_HAND_LEFT) {
            g_blob.c.left_hand_presses++;
        } else if (hand == TS_HAND_RIGHT) {
            g_blob.c.right_hand_presses++;
        }
    }
}

static inline void ts_bump_layer(uint8_t layer) {
    if (layer < TS_MAX_LAYERS) g_blob.c.layer_counts[layer]++;
}

static inline void ts_bump_mods(uint8_t mods) {
    // bucket per mod bit (order LCTL..RGUI) – count currently-held mods at press time
    const uint8_t bits[8] = {MOD_BIT(KC_LCTL), MOD_BIT(KC_LSFT), MOD_BIT(KC_LALT), MOD_BIT(KC_LGUI),
                             MOD_BIT(KC_RCTL), MOD_BIT(KC_RSFT), MOD_BIT(KC_RALT), MOD_BIT(KC_RGUI)};
    for (uint8_t i = 0; i < 8; i++) {
        if (mods & bits[i]) g_blob.c.mod_counts[i]++;
    }
}

#if TS_ENABLE_BIGRAM_STATS
static void ts_record_bigram(uint16_t pos1, uint16_t pos2) {
    if (pos1 == 0xFFFF || pos2 == 0xFFFF || pos1 == pos2) return;

    // Convert to 8-bit indices (may lose precision for very large matrices)
    uint8_t p1 = (uint8_t)(pos1 & 0xFF);
    uint8_t p2 = (uint8_t)(pos2 & 0xFF);

    // Look for existing bigram
    for (uint8_t i = 0; i < g_blob.c.bigram_count; i++) {
        if (g_blob.c.bigrams[i].key1_pos == p1 && g_blob.c.bigrams[i].key2_pos == p2) {
            if (g_blob.c.bigrams[i].count != UINT16_MAX) {
                g_blob.c.bigrams[i].count++;
            }
            return;
        }
    }

    // Add new bigram if space available
    if (g_blob.c.bigram_count < TS_MAX_BIGRAMS) {
        uint8_t idx = g_blob.c.bigram_count++;
        g_blob.c.bigrams[idx].key1_pos = p1;
        g_blob.c.bigrams[idx].key2_pos = p2;
        g_blob.c.bigrams[idx].count = 1;
    }
}
#endif

#if TS_ENABLE_LAYER_TIME
static void ts_update_layer_time(void) {
    uint32_t now = timer_read32();
    if (g_layer_time_start != 0 && g_current_layer < TS_MAX_LAYERS) {
        uint32_t elapsed = now - g_layer_time_start;
        g_blob.c.layer_time_ms[g_current_layer] += elapsed;
    }
    g_layer_time_start = now;
}
#endif

// ---- public ----
void ts_init(void) {
    ts_load();
    g_layer_state_cached = layer_state;
    g_last_flush         = timer_read32();

#if TS_ENABLE_LAYER_TIME
    g_current_layer = get_highest_layer(layer_state | default_layer_state);
    g_layer_time_start = timer_read32();
#endif

    // Initialize session if not already started
    if (g_blob.c.session_start_time == 0) {
        ts_start_new_session();
    }
}

void ts_task_10ms(void) {
    // Poll WPM ~every 50–100ms; cheap to do every scan too.
    static uint32_t last = 0;
    uint32_t        now  = timer_read32();
    if (now - last >= 50) {
        last         = now;
        uint16_t wpm = get_current_wpm();

        // max WPM tracking
        if (wpm > g_blob.c.max_wpm) {
            g_blob.c.max_wpm = wpm;
            g_dirty          = true;
        }

        // Session max WPM
        if (wpm > g_blob.c.session_max_wpm) {
            g_blob.c.session_max_wpm = wpm;
            g_dirty                  = true;
        }

        // EMA
        ts_update_wpm_ema(wpm);
    }

    // Flush policy
    if (g_dirty) {
        if ((now - g_last_flush) >= (TS_FLUSH_SECONDS * 1000UL) || g_event_ctr >= TS_FLUSH_EVENTS) {
            ts_save_if_needed(false);
        }
    }
}

void ts_on_keyevent(keyrecord_t *record, uint16_t keycode) {
    if (!record->event.pressed) return;

    g_blob.c.total_presses++;
    g_blob.c.session_presses++;

    // Position tracking
    uint16_t pos_index = ts_pos_to_index(record->event.key.row, record->event.key.col);
    ts_bump_pos(record->event.key.row, record->event.key.col);

#if TS_ENABLE_BIGRAM_STATS
    // Bigram tracking
    ts_record_bigram(g_last_pos_index, pos_index);
    g_last_pos_index = pos_index;
#endif

    // Active (highest) layer at press time
    uint8_t hl = get_highest_layer(layer_state | default_layer_state);
    ts_bump_layer(hl);

    // Modifiers held at the moment
    ts_bump_mods(get_mods() | get_oneshot_mods());

    g_dirty = true;
    g_event_ctr++;
}

layer_state_t ts_on_layer_change(layer_state_t new_state) {
    g_layer_state_cached = new_state;

#if TS_ENABLE_LAYER_TIME
    ts_update_layer_time();
    g_current_layer = get_highest_layer(new_state | default_layer_state);
#endif

    return new_state;
}

// ---- Basic getters ----
uint16_t ts_get_current_wpm(void) {
    return get_current_wpm();
}

uint16_t ts_get_avg_wpm(void) {
    return g_blob.c.avg_wpm_ema;
}

uint16_t ts_get_max_wpm(void) {
    return g_blob.c.max_wpm;
}

uint32_t ts_get_total_presses(void) {
    return g_blob.c.total_presses;
}

uint32_t ts_get_session_presses(void) {
    return g_blob.c.session_presses;
}

uint16_t ts_get_session_max_wpm(void) {
    return g_blob.c.session_max_wpm;
}

uint32_t ts_get_session_time_minutes(void) {
    if (g_blob.c.session_start_time == 0) return 0;
    return (timer_read32() - g_blob.c.session_start_time) / (60 * 1000);
}

// ---- Hand balance ----
float ts_get_left_hand_ratio(void) {
    uint32_t total_hand_presses = g_blob.c.left_hand_presses + g_blob.c.right_hand_presses;
    if (total_hand_presses == 0) return 0.5f;
    return (float)g_blob.c.left_hand_presses / total_hand_presses;
}

float ts_get_right_hand_ratio(void) {
    return 1.0f - ts_get_left_hand_ratio();
}

// ---- Position queries ----
uint32_t ts_get_pos_presses(uint8_t row, uint8_t col) {
    uint16_t i = ts_pos_to_index(row, col);
    if (i < (MATRIX_ROWS * MATRIX_COLS)) {
        return g_blob.c.pos[i].presses;
    }
    return 0;
}

bool ts_find_most_used_pos(uint16_t *index_out, uint32_t *count_out) {
    uint32_t best  = 0;
    uint16_t idx   = 0;
    bool     found = false;
    for (uint16_t i = 0; i < MATRIX_ROWS * MATRIX_COLS; i++) {
        uint32_t c = g_blob.c.pos[i].presses;
        if (c > best) {
            best  = c;
            idx   = i;
            found = true;
        }
    }
    if (found) {
        if (index_out) *index_out = idx;
        if (count_out) *count_out = best;
    }
    return found;
}

bool ts_find_least_used_pos(uint16_t *index_out, uint32_t *count_out, bool nonzero_only) {
    uint32_t best  = UINT32_MAX;
    uint16_t idx   = 0;
    bool     found = false;
    for (uint16_t i = 0; i < MATRIX_ROWS * MATRIX_COLS; i++) {
        uint32_t c = g_blob.c.pos[i].presses;
        if (nonzero_only && c == 0) continue;
        if (c < best) {
            best  = c;
            idx   = i;
            found = true;
        }
    }
    if (found) {
        if (index_out) *index_out = idx;
        if (count_out) *count_out = (best == UINT32_MAX ? 0 : best);
    }
    return found;
}

// ---- Layer queries ----
uint32_t ts_get_layer_presses(uint8_t layer) {
    if (layer < TS_MAX_LAYERS) {
        return g_blob.c.layer_counts[layer];
    }
    return 0;
}

#if TS_ENABLE_LAYER_TIME
uint32_t ts_get_layer_time_ms(uint8_t layer) {
    if (layer < TS_MAX_LAYERS) {
        return g_blob.c.layer_time_ms[layer];
    }
    return 0;
}

float ts_get_layer_time_ratio(uint8_t layer) {
    if (layer >= TS_MAX_LAYERS) return 0.0f;

    uint32_t total_time = 0;
    for (uint8_t i = 0; i < TS_MAX_LAYERS; i++) {
        total_time += g_blob.c.layer_time_ms[i];
    }

    if (total_time == 0) return 0.0f;
    return (float)g_blob.c.layer_time_ms[layer] / total_time;
}
#endif

int8_t ts_find_most_used_layer(uint8_t *layer_out, uint32_t *count_out) {
    uint32_t best  = 0;
    uint8_t  idx   = 0;
    bool     found = false;
    for (uint8_t i = 0; i < TS_MAX_LAYERS; i++) {
        uint32_t c = g_blob.c.layer_counts[i];
        if (c > best) {
            best  = c;
            idx   = i;
            found = true;
        }
    }
    if (found) {
        if (layer_out) *layer_out = idx;
        if (count_out) *count_out = best;
        return 1;
    }
    return 0;
}

int8_t ts_find_least_used_layer(uint8_t *layer_out, uint32_t *count_out, bool nonzero_only) {
    uint32_t best  = UINT32_MAX;
    uint8_t  idx   = 0;
    bool     found = false;
    for (uint8_t i = 0; i < TS_MAX_LAYERS; i++) {
        uint32_t c = g_blob.c.layer_counts[i];
        if (nonzero_only && c == 0) continue;
        if (c < best) {
            best  = c;
            idx   = i;
            found = true;
        }
    }
    if (found) {
        if (layer_out) *layer_out = idx;
        if (count_out) *count_out = (best == UINT32_MAX ? 0 : best);
        return 1;
    }
    return 0;
}

// ---- Modifier queries ----
uint32_t ts_get_mod_presses(uint8_t modbit_index) {
    if (modbit_index < 8) {
        return g_blob.c.mod_counts[modbit_index];
    }
    return 0;
}

const char* ts_modbit_to_string(uint8_t modbit_index) {
    static const char* mod_names[8] = {
        "LCtrl", "LShift", "LAlt", "LGui",
        "RCtrl", "RShift", "RAlt", "RGui"
    };
    if (modbit_index < 8) {
        return mod_names[modbit_index];
    }
    return "Unknown";
}

int8_t ts_find_most_used_mod(uint8_t *modbit_out, uint32_t *count_out) {
    uint32_t best  = 0;
    uint8_t  idx   = 0;
    bool     found = false;
    for (uint8_t i = 0; i < 8; i++) {
        if (g_blob.c.mod_counts[i] > best) {
            best  = g_blob.c.mod_counts[i];
            idx   = i;
            found = true;
        }
    }
    if (found) {
        if (modbit_out) *modbit_out = idx;
        if (count_out) *count_out = best;
        return 1;
    }
    return 0;
}

int8_t ts_find_least_used_mod(uint8_t *modbit_out, uint32_t *count_out, bool nonzero_only) {
    uint32_t best  = UINT32_MAX;
    uint8_t  idx   = 0;
    bool     found = false;
    for (uint8_t i = 0; i < 8; i++) {
        uint32_t c = g_blob.c.mod_counts[i];
        if (nonzero_only && c == 0) continue;
        if (c < best) {
            best  = c;
            idx   = i;
            found = true;
        }
    }
    if (found) {
        if (modbit_out) *modbit_out = idx;
        if (count_out) *count_out = (best == UINT32_MAX ? 0 : best);
        return 1;
    }
    return 0;
}

#if TS_ENABLE_BIGRAM_STATS
// ---- Bigram queries ----
bool ts_find_most_used_bigram(uint8_t *pos1_out, uint8_t *pos2_out, uint16_t *count_out) {
    uint16_t best = 0;
    uint8_t  best_idx = 0;
    bool     found = false;

    for (uint8_t i = 0; i < g_blob.c.bigram_count; i++) {
        if (g_blob.c.bigrams[i].count > best) {
            best = g_blob.c.bigrams[i].count;
            best_idx = i;
            found = true;
        }
    }

    if (found) {
        if (pos1_out) *pos1_out = g_blob.c.bigrams[best_idx].key1_pos;
        if (pos2_out) *pos2_out = g_blob.c.bigrams[best_idx].key2_pos;
        if (count_out) *count_out = best;
    }

    return found;
}

void ts_get_top_bigrams(ts_bigram_t *output, uint8_t max_count, uint8_t *actual_count) {
    if (!output || max_count == 0) {
        if (actual_count) *actual_count = 0;
        return;
    }

    uint8_t count = 0;
    uint8_t available = g_blob.c.bigram_count < max_count ? g_blob.c.bigram_count : max_count;

    // Simple selection sort for top bigrams
    for (uint8_t i = 0; i < available; i++) {
        uint8_t best_idx = 0;
        uint16_t best_count = 0;

        // Find highest unused bigram
        for (uint8_t j = 0; j < g_blob.c.bigram_count; j++) {
            // Check if already used
            bool used = false;
            for (uint8_t k = 0; k < count; k++) {
                if (output[k].key1_pos == g_blob.c.bigrams[j].key1_pos &&
                    output[k].key2_pos == g_blob.c.bigrams[j].key2_pos) {
                    used = true;
                    break;
                }
            }

            if (!used && g_blob.c.bigrams[j].count > best_count) {
                best_count = g_blob.c.bigrams[j].count;
                best_idx = j;
            }
        }

        if (best_count > 0) {
            output[count] = g_blob.c.bigrams[best_idx];
            count++;
        }
    }

    if (actual_count) *actual_count = count;
}
#endif

// ---- Summary and analysis ----
void ts_get_summary(ts_summary_t *summary) {
    if (!summary) return;

    memset(summary, 0, sizeof(*summary));

    summary->total_lifetime_presses = g_blob.c.total_presses;
    summary->session_presses = g_blob.c.session_presses;
    summary->current_wpm = get_current_wpm();
    summary->avg_wpm = g_blob.c.avg_wpm_ema;
    summary->max_wpm = g_blob.c.max_wpm;
    summary->session_max_wpm = g_blob.c.session_max_wpm;
    summary->left_hand_ratio = ts_get_left_hand_ratio();

    uint8_t layer;
    if (ts_find_most_used_layer(&layer, NULL) > 0) {
        summary->most_used_layer = layer;
    }

    uint8_t mod;
    if (ts_find_most_used_mod(&mod, NULL) > 0) {
        summary->most_used_mod = mod;
    }

    uint16_t pos;
    if (ts_find_most_used_pos(&pos, NULL)) {
        summary->most_used_pos_index = pos;
    }
}

void ts_start_new_session(void) {
    g_blob.c.session_presses = 0;
    g_blob.c.session_start_time = timer_read32();
    g_blob.c.session_max_wpm = 0;
    g_dirty = true;
}

// ---- Advanced analysis ----
uint32_t ts_estimate_consecutive_same_finger(void) {
    // This is a simplified heuristic - in practice you'd need finger mapping
    return g_blob.c.consecutive_same_finger;
}

uint32_t ts_estimate_finger_rolls(void) {
    // This is a simplified heuristic - in practice you'd need finger mapping
    return g_blob.c.finger_rolls;
}

float ts_calculate_key_distribution_entropy(void) {
    // Calculate Shannon entropy of key distribution
    uint32_t total_presses = g_blob.c.total_presses;
    if (total_presses == 0) return 0.0f;

    float entropy = 0.0f;
    for (uint16_t i = 0; i < MATRIX_ROWS * MATRIX_COLS; i++) {
        if (g_blob.c.pos[i].presses > 0) {
            float p = (float)g_blob.c.pos[i].presses / total_presses;
            entropy -= p * log2f(p);
        }
    }

    return entropy;
}

void ts_mark_dirty(void) {
    g_dirty = true;
}

void ts_force_flush(void) {
    ts_save_if_needed(true);
}

void ts_debug_print(void) {
#ifdef CONSOLE_ENABLE
    dprintln("[ts] ---- Typing Stats ----");
    dprintf("[ts] WPM cur=%u avg=%u max=%u session_max=%u\n",
            get_current_wpm(), g_blob.c.avg_wpm_ema, g_blob.c.max_wpm, g_blob.c.session_max_wpm);
    dprintf("[ts] Total presses=%lu Session presses=%lu\n",
            (unsigned long)g_blob.c.total_presses, (unsigned long)g_blob.c.session_presses);
    dprintf("[ts] Hand balance L:%.1f%% R:%.1f%%\n",
            ts_get_left_hand_ratio() * 100, ts_get_right_hand_ratio() * 100);

    uint8_t  ml;
    uint32_t mc;
    if (ts_find_most_used_layer(&ml, &mc)) {
        dprintf("[ts] Most used layer=%u (%lu presses)\n", ml, (unsigned long)mc);
    }

    uint16_t pi;
    uint32_t pc;
    if (ts_find_most_used_pos(&pi, &pc)) {
        uint8_t row, col;
        ts_index_to_pos(pi, &row, &col);
        dprintf("[ts] Most used pos=(%u,%u) idx=%u (%lu presses)\n",
                row, col, pi, (unsigned long)pc);
    }

    uint8_t mod_idx;
    uint32_t mod_count;
    if (ts_find_most_used_mod(&mod_idx, &mod_count) > 0) {
        dprintf("[ts] Most used mod=%s (%lu presses)\n",
                ts_modbit_to_string(mod_idx), (unsigned long)mod_count);
    }
#endif
}

void ts_debug_print_detailed(void) {
#ifdef CONSOLE_ENABLE
    ts_debug_print();

    dprintln("[ts] ---- Layer Details ----");
    for (uint8_t i = 0; i < TS_MAX_LAYERS; i++) {
        if (g_blob.c.layer_counts[i] > 0) {
            dprintf("[ts] Layer %u: %lu presses", i, (unsigned long)g_blob.c.layer_counts[i]);
#if TS_ENABLE_LAYER_TIME
            dprintf(" %.1fs (%.1f%%)",
                    g_blob.c.layer_time_ms[i] / 1000.0f,
                    ts_get_layer_time_ratio(i) * 100);
#endif
            dprintln("");
        }
    }

    dprintln("[ts] ---- Modifier Details ----");
    for (uint8_t i = 0; i < 8; i++) {
        if (g_blob.c.mod_counts[i] > 0) {
            dprintf("[ts] %s: %lu presses\n",
                    ts_modbit_to_string(i), (unsigned long)g_blob.c.mod_counts[i]);
        }
    }

#if TS_ENABLE_BIGRAM_STATS
    dprintln("[ts] ---- Top Bigrams ----");
    ts_bigram_t top_bigrams[5];
    uint8_t count;
    ts_get_top_bigrams(top_bigrams, 5, &count);
    for (uint8_t i = 0; i < count; i++) {
        uint8_t r1, c1, r2, c2;
        ts_index_to_pos(top_bigrams[i].key1_pos, &r1, &c1);
        ts_index_to_pos(top_bigrams[i].key2_pos, &r2, &c2);
        dprintf("[ts] (%u,%u)->(%u,%u): %u\n", r1, c1, r2, c2, top_bigrams[i].count);
    }
#endif

    dprintf("[ts] Key distribution entropy: %.2f bits\n", ts_calculate_key_distribution_entropy());
    dprintf("[ts] Session time: %lu minutes\n", (unsigned long)ts_get_session_time_minutes());
#endif
}

void ts_debug_print_heatmap(void) {
#ifdef CONSOLE_ENABLE
    dprintln("[ts] ---- Key Heatmap ----");

    // Find max for scaling
    uint32_t max_presses = 0;
    for (uint16_t i = 0; i < MATRIX_ROWS * MATRIX_COLS; i++) {
        if (g_blob.c.pos[i].presses > max_presses) {
            max_presses = g_blob.c.pos[i].presses;
        }
    }

    if (max_presses == 0) {
        dprintln("[ts] No key presses recorded yet");
        return;
    }

    // Print heatmap
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        dprintf("[ts] ");
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uint16_t idx = ts_pos_to_index(row, col);
            uint32_t presses = g_blob.c.pos[idx].presses;

            // Scale to 0-9 for display
            uint8_t intensity = (presses * 9) / max_presses;
            if (presses > 0 && intensity == 0) intensity = 1; // Ensure non-zero shows

            dprintf("%u", intensity);
        }
        dprintln("");
    }
    dprintf("[ts] Scale: 0=unused, 9=most used (%lu presses)\n", (unsigned long)max_presses);
#endif
}

#ifdef OLED_ENABLE
void ts_render_oled_stats(void) {
    oled_write_P(PSTR("Typing Stats\n"), false);

    char buf[32];
    snprintf(buf, sizeof(buf), "WPM: %u/%u/%u\n",
             get_current_wpm(), g_blob.c.avg_wpm_ema, g_blob.c.max_wpm);
    oled_write(buf, false);

    snprintf(buf, sizeof(buf), "Keys: %lu\n", (unsigned long)g_blob.c.total_presses);
    oled_write(buf, false);

    snprintf(buf, sizeof(buf), "Session: %lu\n", (unsigned long)g_blob.c.session_presses);
    oled_write(buf, false);

    snprintf(buf, sizeof(buf), "L/R: %.0f%%/%.0f%%\n",
             ts_get_left_hand_ratio() * 100, ts_get_right_hand_ratio() * 100);
    oled_write(buf, false);
}

void ts_render_oled_heatmap(void) {
    // Simple 8x4 visualization of key usage
    oled_write_P(PSTR("Heatmap:\n"), false);

    // Find max for scaling
    uint32_t max_presses = 0;
    for (uint16_t i = 0; i < MATRIX_ROWS * MATRIX_COLS; i++) {
        if (g_blob.c.pos[i].presses > max_presses) {
            max_presses = g_blob.c.pos[i].presses;
        }
    }

    if (max_presses == 0) {
        oled_write_P(PSTR("No data"), false);
        return;
    }

    // Display simplified heatmap
    uint8_t display_rows = MATRIX_ROWS < 4 ? MATRIX_ROWS : 4;
    uint8_t display_cols = MATRIX_COLS < 16 ? MATRIX_COLS : 16;

    for (uint8_t row = 0; row < display_rows; row++) {
        char line[17] = {0}; // 16 chars + null terminator
        for (uint8_t col = 0; col < display_cols; col++) {
            uint16_t idx = ts_pos_to_index(row, col);
            uint32_t presses = g_blob.c.pos[idx].presses;

            // Map to display characters
            if (presses == 0) {
                line[col] = '.';
            } else {
                uint8_t intensity = (presses * 4) / max_presses;
                if (intensity == 0) intensity = 1;
                line[col] = '0' + intensity;
            }
        }
        oled_write(line, false);
        oled_write_P(PSTR("\n"), false);
    }
}
#endif

// ---- internals ----
static void ts_update_wpm_ema(uint16_t wpm) {
    // integer EMA: ema += (alpha)*(wpm - ema)
    uint16_t ema  = g_blob.c.avg_wpm_ema;
    int16_t  diff = (int16_t)wpm - (int16_t)ema;
    ema += (int16_t)((TS_WPM_EMA_ALPHA_NUM * diff) / TS_WPM_EMA_ALPHA_DEN);
    if (ema != g_blob.c.avg_wpm_ema) {
        g_blob.c.avg_wpm_ema = ema;
        g_dirty              = true;
    }
}

static void ts_default_blob(ts_blob_t *b) {
    memset(b, 0, sizeof(*b));
    b->magic   = TS_MAGIC;
    b->version = TS_VERSION;
}

static void ts_load(void) {
    if (g_loaded) return;

    // READ whole blob from user datablock
    eeconfig_read_user_datablock(&g_blob, 0, sizeof(g_blob));

    bool ok = (g_blob.magic == TS_MAGIC && g_blob.version == TS_VERSION);
    if (ok) {
        uint32_t expect = g_blob.crc32;
        g_blob.crc32    = 0;
        uint32_t calc   = crc32_update(0xFFFFFFFFu, (const uint8_t *)&g_blob, sizeof(g_blob));
        g_blob.crc32    = expect;
        ok              = (calc == expect);
    }
    if (!ok) {
        ts_default_blob(&g_blob);
        g_dirty = true;
        ts_save_if_needed(true);
    }
    g_loaded = true;
}

static void ts_save_if_needed(bool force) {
    if (!g_loaded) return;
    if (!g_dirty && !force) return;

#if TS_ENABLE_LAYER_TIME
    // Update current layer time before saving
    ts_update_layer_time();
#endif

    g_blob.crc32 = 0;
    uint32_t crc = crc32_update(0xFFFFFFFFu, (const uint8_t *)&g_blob, sizeof(g_blob));
    g_blob.crc32 = crc;

    // WRITE whole blob to user datablock
    eeconfig_update_user_datablock(&g_blob, 0, sizeof(g_blob));

    g_dirty      = false;
    g_event_ctr  = 0;
    g_last_flush = timer_read32();
}

// Simple CRC-32 (poly 0xEDB88320)
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len) {
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
    }
    return ~crc;
}

void ts_reset_defaults(void) {
    ts_default_blob(&g_blob);
    g_loaded = true;
    g_dirty  = true;

#if TS_ENABLE_LAYER_TIME
    g_layer_time_start = timer_read32();
    g_current_layer = get_highest_layer(layer_state | default_layer_state);
#endif

#if TS_ENABLE_BIGRAM_STATS
    g_last_pos_index = 0xFFFF;
#endif
}

void ts_eeconfig_init_user(void) {
    // Called when QMK detects EEPROM is uninitialized / reset.
    ts_reset_defaults();
    ts_save_if_needed(true); // writes the clean blob via eeconfig_update_user_datablock
}
