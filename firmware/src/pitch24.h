/* SPDX-License-Identifier: GPL-3.0-only */
/* maqam: the 24-tone pitch core (change maqam-24-tone-scales, design D1 / D2 / D13).
 * A synth note byte is a MIDI semitone (bits 0..6) plus a quarter-tone flag (bit 7, +50 cents):
 *     p24 = semitone * 2 + flag.
 * A byte is NOT a MIDI note any more: use note_semi() wherever a semitone is meant (MIDI out, GM maps,
 * sample zones, slice index) and keep the whole byte wherever a note is carried (steps, voices, the
 * mono stack, the arp). Plain 12-tone bytes (flag 0) are unchanged, so every 12-TET path is bit-identical.
 * Included once, after core.h (voice_t); the rest of the quarter-tone logic lands here too. */
#ifndef PITCH24_H
#define PITCH24_H

/* the track colours (user-approved, OKLCH-balanced). Track 1 green, 2 red, 3 yellow, 4 / the drums pink; the knobs, tiles, dials and
 * the visualiser take them through TE_COL / TE_MID / TE_DIM (ui_studio.c) or these. R, G, B:
 *   base  oklch(0.66 0.170 148) #30AD51   oklch(0.64 0.190 28) #E95145   oklch(0.88 0.170 104) #EBDC3A   oklch(0.74 0.150 350) #F080B8
 *   mid   the same hues at 0.74 x lightness, 0.72 x chroma (the old mids' ratio): 0.488 0.122, 0.474 0.137, 0.651 0.122, 0.548 0.108
 *   dim   the same hues at 0.46 x lightness, 0.42 x chroma (the old dims'): oklch(0.304 0.071 148), (0.294 0.080 28), (0.405 0.071 104),
 *         (0.340 0.063 350)
 *   deep  dim x 0.65 (the visualiser's SLOOP sail bands) */
#define TRK_C0 48, 173, 81
#define TRK_C1 233, 81, 69
#define TRK_C2 235, 220, 58
#define TRK_C3 240, 128, 184
#define TRK_M0 32, 114, 53
#define TRK_M1 154, 53, 45
#define TRK_M2 156, 147, 41
#define TRK_M3 159, 85, 122
#define TRK_D0 15, 56, 25
#define TRK_D1 77, 25, 20
#define TRK_D2 79, 75, 22
#define TRK_D3 80, 42, 61
#define TRK_X0 10, 36, 16
#define TRK_X1 50, 16, 13
#define TRK_X2 51, 49, 14
#define TRK_X3 52, 27, 40
#define RGB3(...) RGB(__VA_ARGS__)

#define NOTE_Q 0x80u                                    /* the quarter-tone flag of a note byte */
#define CENTS_MAX 100                                   /* the total offset on a note: +-100 cents */

static inline uint32_t note_semi(uint32_t n) { return n & 127u; }          /* the MIDI semitone */
static inline uint32_t note_q(uint32_t n) { return (n >> 7) & 1u; }        /* 0 / 1: +50 cents */
static inline uint32_t note_p24(uint32_t n) { return note_semi(n) * 2u + note_q(n); }   /* 0..255 */
static inline uint32_t note_from_p24(uint32_t p) { return ((p >> 1) & 127u) | ((p & 1u) << 7); }
static inline int32_t note_p16(uint32_t n) { return (int32_t)(note_semi(n) * 16u + note_q(n) * 8u); }   /* 1/16 semitone */

/* a note shifted by d semitones (the Q flag stays), the semitone clamped to lo..127 */
static inline uint32_t note_shift(uint32_t n, int32_t d, int32_t lo)
{
    int32_t s = (int32_t)note_semi(n) + d;
    return (uint32_t)(s < lo ? lo : s > 127 ? 127 : s) | (n & NOTE_Q);
}

/* cents (clamped to +-100) -> whole 1/16 semitones in *whole16 and the rest in 1/4096 (1 ct = 2.367), as the
 * G_TUNE split in track_render: floor division, so the rest is never negative and there is no dead zone */
static inline int32_t cents_split(int32_t cents, int32_t *whole16)
{
    int32_t w;
    cents = cents < -CENTS_MAX ? -CENTS_MAX : cents > CENTS_MAX ? CENTS_MAX : cents;
    w = cents >= 0 ? cents * 16 / 100 : -((-cents * 16 + 99) / 100);
    *whole16 = w;
    return (cents * 16 - w * 100) * 2367 / 16000;
}

/* the voice's target pitch: semi*16 + q*8 + whole16(cents), the sub-1/16 rest in v->bend_fine.
 * With the flag and the cents 0 this is exactly note * 16 and 0. */
static void pitch24_set(voice_t *v, uint32_t note, int32_t cents)
{
    int32_t w;
    v->bend_fine = (int16_t)cents_split(cents, &w);
    v->pitch16 = note_p16(note) + w;
}

/* ---------------------------------------------------------------- scales (tasks 2.3 .. 2.5a) --- */
#include "scales.h"                                     /* SCALES[] / NSCALES: tools/gen_scales.py */

#define N_SCALE SCALE_CODES                     /* the P_SCALE names: the codes (<= 5 chars), generated with SCALES[] */
static const char *const N_INTON[] = {"EQUAL", "TRAD"};   /* G_INTON */
/* the 24 quarter-tone names from C (ROOT, task 2.4). ASCII for now: 'd' half-flat, '+' half-sharp;
 * the glyphs are task 6.1 */
static const char *const N_NOTE24[24] = {"C", "C+", "C#", "Dd", "D", "D+", "D#", "Ed", "E", "Fd", "F", "F+",
                                         "F#", "Gd", "G", "G+", "G#", "Ad", "A", "A+", "A#", "Bd", "B", "Cd"};

/* the same on the screen: "\x01" is the half-flat glyph, "\x02" the half-sharp (gfx.c cv_text: codes 1 and 2, in both fonts) */
static const char *const N_NOTE24G[24] = {"C", "C\x02", "C#", "D\x01", "D", "D\x02", "D#", "E\x01", "E", "F\x01", "F", "F\x02",
                                          "F#", "G\x01", "G", "G\x02", "G#", "A\x01", "A", "A\x02", "A#", "B\x01", "B", "C\x01"};

static inline uint32_t scale_index(const track_t *t)
{
    int32_t s = t->p[P_SCALE];
    return (uint32_t)(s < 0 ? 0 : s >= NSCALES ? NSCALES - 1 : s);
}
/* the 12-bit semitone view of a 24-bit mask (bit i = slot 2i): what the 12-tone code (kb_map, chord_notes,
 * the SCL tiles) reads until the 24-grid rewrite of tasks 3.x. Exact for WEST; quarter-tone degrees drop out */
static inline uint32_t mask12_of(uint32_t m24)
{
    uint32_t i, m = 0;
    for (i = 0; i < 12u; i++)
        m |= ((m24 >> (2u * i)) & 1u) << i;
    return m;
}
static inline int32_t root_semi(const track_t *t) { return t->p[P_ROOT] >> 1; }   /* the root's semitone (P_ROOT is 0..23) */

/* set while a note comes from MIDI in: it plays as a plain 12-tone note, with no intonation offset */
static uint8_t p24_plain;
/* the intonation of a note (design D4 / D14): tune[(p24 - root) mod 24] when the slot is in the scale (either
 * form) and G_INTON is TRADITIONAL, in cents; 0 for everything else (WEST: all zero) */
static int32_t p24_tune(const track_t *t, uint32_t note)
{
    const scale_def_t *sc;
    uint32_t slot;
    if (!song.g[G_INTON] || p24_plain)
        return 0;
    sc = &SCALES[scale_index(t)];
    slot = (note_p24(note) + 48u - (uint32_t)t->p[P_ROOT]) % 24u;
    return ((sc->asc | sc->desc) >> slot) & 1u ? sc->tune[slot] : 0;
}
/* a note's total offset: the step bend + the intonation, clamped to +-100 */
static inline int32_t p24_cents(const track_t *t, uint32_t note, int32_t cents)
{
    cents += p24_tune(t, note);
    return cents < -CENTS_MAX ? -CENTS_MAX : cents > CENTS_MAX ? CENTS_MAX : cents;
}

/* ---- scale sets in the SCL layer (task 4.1, D8): the set follows from the scale, nothing more is saved ---- */
static const char *const N_SET[NSETS] = {"west", "turk", "arab", "india"};
/* flat spellings for the home hint (ASCII: 'd' half-flat, '+' half-sharp) */
static const char *const N_NOTE24F[24] = {"C", "C\x02", "Db", "D\x01", "D", "D\x02", "Eb", "E\x01", "E", "F\x01", "F", "F\x02",
                                          "Gb", "G\x01", "G", "G\x02", "Ab", "A\x01", "A", "A\x02", "Bb", "B\x01", "B", "C\x01"};
static uint32_t scale_set_first(uint32_t set)           /* the first scale of a set */
{
    uint32_t i;
    for (i = 0; i < NSCALES; i++)
        if (SCALES[i].set == set)
            return i;
    return 0;
}
static uint32_t scale_set_count(uint32_t set)
{
    uint32_t i, n = 0;
    for (i = 0; i < NSCALES; i++)
        n += SCALES[i].set == set;
    return n;
}
static uint32_t scale_next_set(uint32_t cur, int32_t dir)   /* PRESETS: the first scale of the next / previous set, wrapping */
{
    uint32_t set = SCALES[cur < NSCALES ? cur : 0].set;
    set = (set + NSETS + (uint32_t)(dir > 0 ? 1 : NSETS - 1)) % NSETS;
    return scale_set_first(set);
}
/* the members of a set need not be contiguous (scales added later are appended after the frozen 52) */
static uint32_t scale_set_rank(uint32_t cur)             /* the place of a scale within its set, 0 .. n-1 */
{
    uint32_t i, r = 0, set = SCALES[cur < NSCALES ? cur : 0].set;
    for (i = 0; i < cur && i < NSCALES; i++)
        r += SCALES[i].set == set;
    return r;
}
static uint32_t scale_step(uint32_t cur, int32_t steps)   /* KNOB 2: steps through the scales of the current set, wrapping */
{
    uint32_t set = SCALES[cur < NSCALES ? cur : 0].set, n = scale_set_count(set), i, r;
    int32_t pos = (int32_t)scale_set_rank(cur) + steps % (int32_t)n;
    r = (uint32_t)((pos + (int32_t)n) % (int32_t)n);
    for (i = 0; i < NSCALES; i++)
        if (SCALES[i].set == set && r-- == 0u)
            return i;
    return cur;
}

/* a signed cent value as text: "-5", "+12", "0" */
static void cents_str(char *b, int32_t c)
{
    uint32_t i = 0, u = (uint32_t)(c < 0 ? -c : c);
    if (c)
        b[i++] = c < 0 ? '-' : '+';
    if (u >= 10u)
        b[i++] = (char)('0' + u / 10u);
    b[i++] = (char)('0' + u % 10u);
    b[i] = 0;
}

/* the SEL layer's subtitle: "Ed arab sikah trad" (the title says "key"; lower case; 176 px is all there is); a scale whose forms differ
 * gets an arrow after its code: up or down as the last key of the selected track went (desc) */
static void scl_sub(char *sub, uint32_t n, uint32_t root24, uint32_t scale, uint32_t desc)
{
    const scale_def_t *sc = &SCALES[scale < NSCALES ? scale : NSCALES - 1u];
    const char *src[7];
    uint32_t i, k = 0, j;
    src[0] = N_NOTE24G[root24 % 24u];
    src[1] = " ";
    src[2] = N_SET[sc->set];
    src[3] = " ";
    src[4] = sc->code;
    src[5] = sc->asc == sc->desc ? "" : desc ? "\x04" : "\x03";   /* the arrow (gfx.c codes 3, 4): a scale with two forms, the direction last played */
    src[6] = song.g[G_INTON] ? " trad" : " equal";
    for (i = 0; i < 7u; i++)
        for (j = 0; src[i][j] && k + 1u < n; j++) {
            char c = src[i][j];
            sub[k++] = (i == 4u) && c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
        }
    sub[k] = 0;
}
/* the traditional tonic of a non-WEST scale (D15), as the label of the scale knob: "home: Bb"; "" for WEST */
static void scl_home(char *d, uint32_t n, uint32_t scale)
{
    const scale_def_t *sc = &SCALES[scale < NSCALES ? scale : NSCALES - 1u];
    const char *src[2];
    uint32_t i, k = 0, j;
    src[0] = "home ";
    src[1] = N_NOTE24F[sc->home % 24u];
    if (sc->set == SET_WEST)
        n = 1;
    for (i = 0; i < 2u && sc->set != SET_WEST; i++)
        for (j = 0; src[i][j] && k + 1u < n; j++)
            d[k++] = src[i][j];
    d[k] = 0;
}

/* ------------------------------------------------------- microtonal key input (tasks 3.1 .. 3.7) --- */
#define P24_SILENT 255u                                 /* = KB_SILENT (seq.c) */
static uint8_t kb_last_key[NTRK];                       /* per track: the last key pressed (D5: direction) */
static uint32_t kb_last_ms[NTRK];                       /* when that key was pressed (fm1_ms): after 1 s the next key counts as ascending */
static uint8_t kb_walk_desc[NTRK];                      /* the direction of the last key pressed on each track (the SEL arrow) */
#define KB_DIR_PAUSE_MS 1000u
static uint8_t kb_desc_now;                             /* the direction kb_map last used (chord_notes follows it) */

static inline uint32_t scale_dir_mask(const scale_def_t *sc, uint32_t desc) { return desc ? sc->desc : sc->asc; }
/* the highest degree at or below slot p (absolute 24-grid) of mask, a scale rooted at slot root */
static int32_t scale_floor(uint32_t mask, int32_t root, int32_t p)
{
    int32_t g = 48;
    while (g-- > 0 && !((mask >> (uint32_t)((((p - root) % 24) + 24) % 24)) & 1u))
        p--;
    return p;
}
/* SNAP: the degree of mask (a form, rooted at slot root) for the key whose quarter-tone window is [c-1, c+1]: the exact slot c if it is
 * a degree, else a degree in the window (both: the one in the direction of travel), else going up the next degree above the window,
 * going down the next below. A WEST scale has no odd slot: ascending a key between two degrees plays the one above, descending the
 * one below (descending is what SNAP always did) */
static int32_t scale_snap(uint32_t mask, int32_t root, int32_t c, uint32_t up)
{
    int32_t d;
#define SB(P) ((mask >> (uint32_t)((((P) - root) % 24 + 24) % 24)) & 1u)
    if (SB(c))
        return c;
    if (SB(c - 1) && SB(c + 1))
        return up ? c + 1 : c - 1;
    if (SB(c + 1))
        return c + 1;
    if (SB(c - 1))
        return c - 1;
    for (d = 2; d < 48; d++) {
        if (up && SB(c + d))
            return c + d;
        if (!up && SB(c - d))
            return c - d;
    }
#undef SB
    return c;
}
/* OCT- / OCT+ held on a synth key: -1 / +1 quarter tone (both: 0); block = the layer buttons that use OCT otherwise */
static inline int32_t oct_mod(uint32_t buttons, uint32_t dn, uint32_t up, uint32_t block)
{
    uint32_t d = (buttons & dn) != 0u, u = (buttons & up) != 0u;
    if (buttons & block)
        return 0;
    return (int32_t)u - (int32_t)d;
}
static inline int32_t clamp_p24(int32_t p) { return p < 0 ? 0 : p > 254 ? 254 : p; }

static inline int32_t pop24(uint32_t m) { int32_t c = 0; while (m) { c += (int32_t)(m & 1u); m >>= 1; } return c; }

/* key k (0..26, F3 = 53 up) -> a synth note byte: OFF chromatic, SNAP (round down from 2*semi + 1 in the
 * 24-bit mask, the modifier before the rounding: D7), WHITE (the white keys walk the degrees from C4 = the
 * root; mod after). desc picks the descending mask (D5). P24_SILENT: a black key in WHITE */
static uint32_t kb_quant(const track_t *t, uint32_t k, uint32_t desc, int32_t mod, int32_t xoct)
{
    static const int8_t DEGREE[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    int32_t n = 53 + (int32_t)k, root = t->p[P_ROOT], p;
    if (t->p[P_QUANT] == 1 && !t->p[P_CHORD]) {
        n += 12 * xoct + t->p[P_TRANS];
        p = scale_snap(scale_dir_mask(&SCALES[scale_index(t)], desc), root, 2 * n + mod, !desc);
    } else if (t->p[P_QUANT] == 2 || t->p[P_CHORD]) {
        const scale_def_t *sc = t->p[P_CHORD] && !t->p[P_SCALE] ? &SCALES[2] : &SCALES[scale_index(t)];
        /* the layout of the white keys comes from the form with more degrees, so it never shifts with the
         * direction (Yaman 6 up / 7 down, Asavari 5 / 7); a degree missing in the direction played is replaced
         * by the next degree of that direction's form: above it going up, below it going down */
        uint32_t dmask = scale_dir_mask(sc, desc), mask = pop24(sc->desc) > pop24(sc->asc) ? sc->desc :
                         pop24(sc->asc) > pop24(sc->desc) ? sc->asc : dmask, i;
        int32_t count = 0, degree = DEGREE[n % 12], oct;
        if (degree < 0)
            return P24_SILENT;
        degree += (n / 12 - 5) * 7;                     /* C4 is the root; the degrees go on below it too */
        for (i = 0; i < 24u; i++)
            count += (mask >> i) & 1u;
        oct = degree / count;
        degree %= count;
        if (degree < 0) {
            degree += count;
            oct--;
        }
        for (i = 0; i < 24u; i++)
            if ((mask >> i) & 1u) {
                if (!degree)
                    break;
                degree--;
            }
        if (!((dmask >> i) & 1u)) {                     /* not in this direction's form: the next degree of it, in the direction played */
            int32_t d;
            for (d = 1; d < 24; d++) {
                int32_t j = desc ? (int32_t)i - d : (int32_t)i + d;
                if ((dmask >> (uint32_t)((j + 48) % 24)) & 1u) {
                    i = (uint32_t)j;                    /* (may go below 0 or past 23: the octave below / above) */
                    break;
                }
            }
        }
        p = 120 + root + 24 * oct + (int32_t)i + 2 * (12 * xoct + t->p[P_TRANS]) + mod;
    } else {
        p = 2 * (n + 12 * xoct + t->p[P_TRANS]) + mod;
    }
    return note_from_p24((uint32_t)clamp_p24(p));
}

/* the scale degree `deg` steps up from note n (a byte, Q kept) in mask; the result as a byte (255: past 127.5) */
static uint32_t degree_up(uint32_t mask, int32_t root, uint32_t n, int32_t deg)
{
    int32_t m = (int32_t)note_p24(n), guard = 96;
    while (deg > 0 && guard--) {
        m++;
        if ((mask >> (uint32_t)((((m - root) % 24) + 24) % 24)) & 1u)
            deg--;
    }
    return m < 256 ? note_from_p24((uint32_t)m) : P24_SILENT;
}

/* SCL tiles and key lights: key k is "in the scale" when its semitone or a quarter tone next to it is a degree (SNAP
 * rounds the key's quarter tone above down, so the E key plays Rast's E-half-flat just below it);
 * root: the key that holds the root slot */
static inline uint32_t key_in_scale(const track_t *t, uint32_t k, uint32_t root_only)
{
    uint32_t mask = scale_dir_mask(&SCALES[scale_index(t)], 0), d;
    d = (uint32_t)((((2 * (53 + (int32_t)k) - t->p[P_ROOT]) % 24) + 24) % 24);
    if (root_only)
        return d == 0u || d == 1u || d == 23u;           /* (an odd root: both keys next to it) */
    return ((mask >> d) & 1u) | ((mask >> ((d + 1u) % 24u)) & 1u) | ((mask >> ((d + 23u) % 24u)) & 1u);
}

/* notes that came in over MIDI, per track (a bit per note): when the arp plays one later it stays plain 12-tone,
 * with no intonation offset ("MIDI in stays 12-tone"). A key pressed on the same note clears the mark */
static uint32_t p24_midi_n[NTRK][8];                    /* 256 bits: a note byte with its quarter-tone bit */
static inline void p24_midi_mark(const track_t *t, uint32_t note, uint32_t on)
{
    uint32_t *w = &p24_midi_n[(uint32_t)(t - trk) % NTRK][(note >> 5) & 7u];
    *w = on ? *w | 1u << (note & 31u) : *w & ~(1u << (note & 31u));
}
static inline uint32_t p24_midi_get(const track_t *t, uint32_t note)
{
    return (p24_midi_n[(uint32_t)(t - trk) % NTRK][(note >> 5) & 7u] >> (note & 31u)) & 1u;
}

/* OCT- / OCT+ on a synth track change the octave on release, only if no key was pressed while held; both held
 * reset it to 0 (task 3.4). Called every UI frame with the buttons' level, whether a key went down this frame,
 * and whether the buttons are free (no layer, hold or screen of its own uses them). Returns the octave step
 * (-1 / 0 / +1), *reset = 1 for both held */
static uint8_t oct_used[2], oct_pend[2];
static void oct_use(void) { oct_used[0] = oct_used[1] = 1; }   /* a knob turned with them held: a modifier too */
static int32_t oct_tick(uint32_t dn, uint32_t up, uint32_t keys, uint32_t free, uint32_t *reset)
{
    static uint8_t pd, pu;
    uint8_t *pend = oct_pend, *used = oct_used;
    int32_t d = 0;
    *reset = 0;
    if (!free) {
        pend[0] = pend[1] = 0;
    } else {
        if (keys) {                                     /* a key while held: it is a modifier, not an octave change */
            used[0] |= (uint8_t)dn;
            used[1] |= (uint8_t)up;
        }
        if (dn && !pd) {
            if (up) { *reset = 1; used[0] = used[1] = 1; pend[0] = pend[1] = 0; }
            else { pend[0] = 1; used[0] = 0; }
        }
        if (up && !pu) {
            if (dn) { *reset = 1; used[0] = used[1] = 1; pend[0] = pend[1] = 0; }
            else { pend[1] = 1; used[1] = 0; }
        }
        if (!dn && pd && pend[0] && !used[0])
            d = -1;
        if (!up && pu && pend[1] && !used[1])
            d = 1;
        if (!dn)
            pend[0] = 0;
        if (!up)
            pend[1] = 0;
    }
    pd = (uint8_t)(dn != 0u);
    pu = (uint8_t)(up != 0u);
    return d;
}

/* ---- the editor's step bytes (protocol v11): n, note0..3 (7 bit), time, flags, vel, lvl, hi, rat, then qbits (bit k:
 * note k is a quarter tone up). 12 bytes. A step's bend is a CENT lock (the lock messages), not a step byte. An older
 * editor sends the first 8 or 11: a note it did not change then keeps its quarter-tone flag ---- */
#define STEP_WIRE 12u
static uint32_t step_wire_out(const step_t *st, uint8_t *o)
{
    uint32_t k, q = 0;
    o[0] = st->n;
    for (k = 0; k < 4u; k++) {
        o[1 + k] = st->note[k] & 0x7Fu;
        q |= (uint32_t)note_q(st->note[k]) << k;
    }
    o[5] = st->time;
    o[6] = st->flags;
    o[7] = st->vel;
    o[8] = st->lvl & 0x7Fu;                               /* v5: 4 x 2-bit fields, the top bit below */
    o[9] = (uint8_t)(st->lvl >> 7 | (st->rat >> 7) << 1);
    o[10] = st->rat & 0x7Fu;
    o[11] = (uint8_t)q;
    return STEP_WIRE;
}
static void step_wire_in(step_t *st, const uint8_t *a, uint32_t na)   /* na: bytes from n on (8 .. 12) */
{
    uint32_t k;
    uint8_t old[4];
    memcpy(old, st->note, 4);
    st->n = (uint8_t)(a[0] > 4u ? 4u : a[0]);
    for (k = 0; k < 4u; k++) {
        st->note[k] = a[1 + k] & 0x7Fu;
        if (na >= 12u ? (a[11] >> k) & 1u : ((old[k] & 0x7Fu) == st->note[k] && note_q(old[k])))
            st->note[k] |= NOTE_Q;
    }
    st->time = (uint8_t)(a[5] > ST_REST ? ST_REST : a[5]);
    st->flags = a[6] & (SF_ACCENT | SF_SLIDE);
    st->vel = a[7] & 0x7Fu;
    if (na >= 11u) {
        st->lvl = (uint8_t)((a[8] & 0x7Fu) | (a[9] & 1u) << 7);
        st->rat = (uint8_t)((a[10] & 0x7Fu) | ((a[9] >> 1) & 1u) << 7);
    } else {
        st->lvl = st->rat = 0;
    }
}

#endif
