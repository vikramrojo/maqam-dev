/* SPDX-License-Identifier: GPL-3.0-only
 * maqam (MAQAM_PRESS, off by default): pressure on held keys. The method is fm1-velocity's (Charles Vestal,
 * GPL-3.0-only, github.com/charlesvestal/fm1-velocity), on Felucca's HAL (Leo Kuroshita, Hügelton Instruments).
 *
 * Two matrix rows are also SARADC inputs: PA7 (ch 0, notes 9..19) and PA8 (ch 1, notes 0..8 and OCT- / OCT+).
 * Notes 20..26 (row PA6) have no converter: they never report pressure. While a key is held (the digital
 * scan's fm1_in.notes), the tick that latches its column drives its row high at a weak pin drive (DRIVE: the
 * pin's current-limited driver against the carbon pill, an ohmmeter) and starts a conversion; the next tick
 * takes it and puts the row back to an input with its pull-up, before the rows are read. PA7 is sampled on
 * even frames, PA8 on odd ones: a held key every two frames (~2.2 ms). The reading falls as the key is pressed
 * harder. The lowest of the first FM1_PRESS_LAND readings is the key's landing; fm1_press[key] is how far the
 * reading has fallen below it since, 0 .. 127 at FM1_PRESS_RANGE counts. Relative to the landing, so no
 * calibration per unit or per key. The key's own LED is dark for its sample (lit, it lifts the reading by ~25
 * counts, and it lights only after the landing).
 *
 * The converter belongs to the tick, so the main loop reads the MASTER pot and the battery here:
 * fm1_press_master (the slot of odd frames, column 0, PA8 = OCT-, as fm1-velocity does) and fm1_press_batt (every
 * FM1_PRESS_BATT_FRAMES, odd frames, column 1, PA8 = OCT+, with the SDK's slow settings; the samples wait
 * while it converts). 0xFFFF: none yet.
 *
 * Included before fm1_input.h (felucca.c): it sets fm1_input.h's two sense hooks. FM1_PRESS_HOST: only the
 * arithmetic (tests/press_test.c). */
#pragma once
#include <stdint.h>

#define FM1_PRESS_RANGE 64u          /* counts below the landing: full pressure (fm1-velocity's d_at_range) */
#define FM1_PRESS_LAND 2u            /* driven readings that set the landing (the lowest of them) */
#define FM1_PRESS_NKEY 20u           /* notes 0..19 (F3..C5): the keys on rows PA7 / PA8 */
#define FM1_PRESS_BATT_FRAMES 512u   /* the battery every ~0.56 s */

/* one driven reading v of a held key: its landing / count, the pressure 0..127 */
static inline uint32_t fm1_press_calc(uint16_t *land, uint8_t *n, uint32_t v)
{
    if (*n < FM1_PRESS_LAND) {
        if (!*n || v < *land)
            *land = (uint16_t)v;
        (*n)++;
        return 0;
    }
    if (v >= *land)
        return 0;
    v = *land - v;
    return v >= FM1_PRESS_RANGE ? 127u : (127u * v + FM1_PRESS_RANGE / 2u) / FM1_PRESS_RANGE;
}

#ifdef FM1_PRESS_HOST
static volatile uint8_t fm1_press[27];       /* (the host: set by the test) */
#else
#include "fm1_gpio.h"
#include "fm1_time.h"

static void fm1_press_take(void);
static void fm1_press_start(uint32_t col);
#define FM1_INPUT_SENSE_TAKE() fm1_press_take()
#define FM1_INPUT_SENSE_START(col) fm1_press_start(col)
#include "fm1_input.h"
#include "fm1_adc.h"

static volatile uint8_t fm1_press[27];
static volatile uint16_t fm1_press_master = 0xFFFFu, fm1_press_batt = 0xFFFFu;
static struct {
    uint8_t pend, col, rowbit, driven, batt, wait;
    uint8_t n[FM1_PRESS_NKEY];
    uint16_t land[FM1_PRESS_NKEY];
    uint32_t frame;
} fm1__pr;

/* fm1-velocity's fm1_adc.h: CON bits 0-2 the clock divider, 3 set, 4 enable, 6 start / clear, 7 done, 8-11 the
 * channel, 12-15 the start delay. Divider 7, delay 2: ~60 us, within a tick (a smaller divider reads the rails) */
#define FM1_PRESS_CON ((uint32_t)7u | 8u | (2u << 12))
#define FM1_PRESS_CON_SDK ((uint32_t)6u | 8u | (15u << 12))   /* fm1_adc_read's */
static void fm1__pr_adc_start(uint32_t ch, uint32_t con)
{
    FM1_ADC_CON = 0;
    if (FM1_WLA_CON0 & (1u << 14))
        FM1_WLA_CON0 &= ~(1u << 14);
    FM1_ADC_CON = (con & 0xF00Fu) | 0x40u | ((ch & 0xFu) << 8);
    FM1_ADC_CON |= 0x10u;
    FM1_ADC_CON |= 0x40u;
}
static inline uint32_t fm1__pr_adc_done(void) { return (FM1_ADC_CON & 0x80u) != 0u; }
static inline uint32_t fm1__pr_adc_take(void)
{
    uint32_t v = FM1_ADC_RES & 0x3FFu;
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
    return v;
}
static inline void fm1__pr_adc_abort(void)
{
    FM1_ADC_CON = 0x40u;
    FM1_ADC_CON = 0;
}
/* fm1-velocity's fm1__row_drive / release: rowbit 3 = PA7, 4 = PA8; HD0 / HD = strength 1 (its DRIVE 2) */
static void fm1__pr_drive(uint32_t rowbit)
{
    uint32_t m = 1u << (rowbit == 3u ? 7 : 8);
    FM1_PR(FM1_PA, FM1_HD0) |= m;
    FM1_PR(FM1_PA, FM1_HD) &= ~m;
    FM1_PR(FM1_PA, FM1_OUT) |= m;
    FM1_PR(FM1_PA, FM1_DIR) &= ~m;
}
static void fm1__pr_release(uint32_t rowbit)
{
    uint32_t m = 1u << (rowbit == 3u ? 7 : 8);
    FM1_PR(FM1_PA, FM1_DIR) |= m;                  /* an input with its pull-up again */
    FM1_PR(FM1_PA, FM1_OUT) &= ~m;
    FM1_PR(FM1_PA, FM1_HD0) &= ~m;
    FM1_PR(FM1_PA, FM1_HD) &= ~m;
}

/* the tick, before the rows are read: the conversion started last tick */
static void fm1_press_take(void)
{
    uint32_t v, key;
    int id;
    if (!fm1__pr.pend)
        return;
    if (fm1__pr.batt) {                            /* the battery's slow conversion: the samples wait */
        if (!fm1__pr_adc_done()) {
            if (++fm1__pr.wait < 20u)
                return;
            fm1__pr_adc_abort();                   /* (2 ms: give up, the next one in a while) */
        } else {
            fm1_press_batt = (uint16_t)fm1__pr_adc_take();
        }
        fm1__pr.pend = fm1__pr.batt = 0;
        return;
    }
    fm1__pr.pend = 0;
    if (fm1__pr.driven) {
        fm1__pr.driven = 0;
        fm1__pr_release(fm1__pr.rowbit);
    }
    if (!fm1__pr_adc_done()) {
        fm1__pr_adc_abort();
        return;
    }
    v = fm1__pr_adc_take();
    if (!fm1__pr.rowbit) {
        fm1_press_master = (uint16_t)v;
        return;
    }
    id = FM1_KEYMAP[fm1__pr.rowbit][fm1__pr.col];
    key = (uint32_t)id - 14u;
    if (id < 14 || key >= FM1_PRESS_NKEY)
        return;
    if (!((fm1_in.notes >> key) & 1u)) {           /* let go meanwhile */
        fm1__pr.n[key] = 0;
        fm1_press[key] = 0;
        return;
    }
    fm1_press[key] = (uint8_t)fm1_press_calc(&fm1__pr.land[key], &fm1__pr.n[key], v);
}

/* the tick, column col just latched (its LEDs lit): this tick's conversion */
static void fm1_press_start(uint32_t col)
{
    uint32_t odd, rowbit, key;
    int id;
    if (fm1__pr.pend)                              /* (the battery converting) */
        return;
    if (col == 0u)
        fm1__pr.frame++;
    odd = fm1__pr.frame & 1u;
    fm1__pr.col = (uint8_t)col;
    if (odd && col == 0u) {                        /* the MASTER pot */
        fm1__pr.rowbit = 0;
        fm1__pr_adc_start(FM1_ADC_MASTER, FM1_PRESS_CON);
        fm1__pr.pend = 1;
        return;
    }
    if (odd && col == 1u) {
        if (fm1__pr.frame % FM1_PRESS_BATT_FRAMES == 1u) {
            fm1__pr.batt = 1;
            fm1__pr.wait = 0;
            fm1__pr_adc_start(FM1_ADC_BATT, FM1_PRESS_CON_SDK);
            fm1__pr.pend = 1;
        }
        return;
    }
    rowbit = odd ? 4u : 3u;
    id = FM1_KEYMAP[rowbit][col];
    key = (uint32_t)id - 14u;
    if (id < 14 || key >= FM1_PRESS_NKEY)
        return;
    if (!((fm1_in.notes >> key) & 1u)) {           /* open: no sample, a fresh landing next time */
        fm1__pr.n[key] = 0;
        fm1_press[key] = 0;
        return;
    }
    fm1__led_lines(fm1_led[col] & ~(1u << rowbit));   /* its LED dark for the sample */
    fm1__pr_drive(rowbit);
    fm1__pr.driven = 1;
    fm1__pr.rowbit = (uint8_t)rowbit;
    fm1__pr_adc_start(rowbit == 3u ? 0u : 1u, FM1_PRESS_CON);
    fm1__pr.pend = 1;
}
#endif
