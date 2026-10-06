/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP host / web emulator HAL: what felucca.c gets from firmware/hal/fm1_*.h, as RAM. Single-threaded.
 * A simulated FM-1: time (ms and TIMER4 ticks), buttons / keys / encoders set by the host, the LED
 * matrix, a 240x240 framebuffer (RGB565, byte-swapped as the panel takes it), the SARADC, and the 1 MiB
 * NOR flash with st_read / st_erase / st_prog (felucca.c) and the XIP window. Included by sloop_host.c
 * before any firmware source. Freestanding: stdint only. */
#pragma once
#include <stdint.h>
#define FM1_TICKS_PER_US 24u

/* ---- time: the host sets fm1_ms (core.h) and these ticks (24 MHz) together */
static uint32_t host_ticks;
static uint32_t fm1_ticks(void) { return host_ticks; }
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
static void fm1_wdt_feed(void) {}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }

/* ---- audio: the I2S half-buffer registers audio.c names (its ISR never runs here; sloop_host.c renders) */
#define FM1_AUDIO_HALF 1u
static uint8_t fm1_audio_pending(void) { return 0; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static void fm1_audio_ack_half(void) {}
static uint32_t fm1_audio_free_half(void) { return 0; }
static void fm1_audio_init(int32_t *buf, uint32_t words, void (*isr)(void), uint32_t prio) { (void)buf; (void)words; (void)isr; (void)prio; }

/* ---- SARADC: MASTER pot (the page), battery (no reading: the device's "no ADC" path) */
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t host_adc_master = 1023;
static int32_t fm1_adc_read(int ch) { return ch == FM1_ADC_MASTER ? host_adc_master : -1; }

/* ---- panel: held buttons (bit = matrix id 0..13) and keys, their edges, encoder detents */
static volatile struct { uint32_t notes, buttons; } fm1_in;
static uint32_t host_btn_press, host_btn_rel, host_note_press;
static int32_t host_enc[7];
static int32_t fm1_enc_take(uint32_t e) { int32_t s = host_enc[e]; host_enc[e] = 0; return s; }
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = host_btn_press;
    if (released)
        *released = host_btn_rel;
    host_btn_press = host_btn_rel = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void) { uint32_t p = host_note_press; host_note_press = 0; return p; }

/* ---- LEDs: the matrix as fm1_input.h has it (column -> packed row bits) */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {         /* key id (0..13 buttons, 14..40 keys) at (row, col) */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_bg[FM1_NCOL];
static volatile uint16_t fm1_led_bg_ns;
static void fm1_led_key(uint32_t id, int on)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                if (on)
                    fm1_led[p] |= (uint8_t)(1u << r);
                else
                    fm1_led[p] &= (uint8_t)~(1u << r);
            }
}
/* the LEDs of a layer as ids: bit i = key id i (0..40), from the column bytes */
static uint64_t host_led_ids(const uint8_t *col)
{
    uint64_t m = 0;
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (((col[p] >> r) & 1u) && FM1_KEYMAP[r][p] >= 0)
                m |= (uint64_t)1 << FM1_KEYMAP[r][p];
    return m;
}

/* ---- LCD: gfx.c draws through these. The framebuffer holds the panel's byte order; fb_blits counts the
 * calls that changed a pixel (the page redraws when it moves) */
static uint16_t host_fb[240 * 240];
static uint32_t host_blits;
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j, ch = 0;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++) {
            uint16_t *d = &host_fb[(y + j) * 240u + x + i];
            ch |= *d != p[j * w + i];
            *d = p[j * w + i];
        }
    host_blits += ch;
}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j, ch = 0;
    uint16_t s = (uint16_t)((c >> 8) | (c << 8));
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++) {
            uint16_t *d = &host_fb[(y + j) * 240u + x + i];
            ch |= *d != s;
            *d = s;
        }
    host_blits += ch;
}

/* ---- flash: 1 MiB NOR, erased 0xFF, programming only clears bits; a bit per 4 KiB sector written since
 * host_flash_clean. The map is the device's (fm1_flash.h): the store 0x97000..0xE0000, the globals 0xFC000. */
#define HOST_FLASH_SIZE (1u << 20)
static uint8_t host_flash[HOST_FLASH_SIZE];
static uint32_t host_flash_dirty[8];
static int host_flash_inited;
static void host_flash_init(void);
#define FL_DATA_LO 0x00097000u
#define FL_DATA_HI 0x000E0000u
#define FL_GLOB_LO 0x000FC000u
#define FL_GLOB_HI 0x000FF000u
#define FL_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && \
                               (uint32_t)(n) <= (hi) - (uint32_t)(off))
#define FL_STORE_OK(off, n) (FL_IN(off, n, FL_DATA_LO, FL_DATA_HI) || FL_IN(off, n, FL_GLOB_LO, FL_GLOB_HI))
static void host_flash_erase_all(void)
{
    uint32_t i;
    for (i = 0; i < HOST_FLASH_SIZE; i++)
        host_flash[i] = 0xFF;
    for (i = 0; i < 8u; i++)
        host_flash_dirty[i] = 0;
}
static void host_flash_touch(uint32_t off, uint32_t n)
{
    uint32_t s;
    for (s = off >> 12; n && s <= (off + n - 1u) >> 12; s++)
        host_flash_dirty[s >> 5] |= 1u << (s & 31u);
}
#define FL_FAR(fn) (fn)                                 /* (the device runs its flash driver from RAM) */
static uint32_t fl_jedec_ram(void) { return 0x856014u; }     /* the expected 1 MiB part */
static void fl_plain_window_init(void) {}
/* samples play in place: XIP offset -> the simulated flash (the device reads 0x02000000 + off - 0x4000).
 * The real header is read first (renamed) so eng_sample.c's include of it is a no-op. */
#define fm1_xip_ptr fm1_xip_ptr_dev
#include "../../firmware/hal/fm1_xip.h"
#undef fm1_xip_ptr
static const uint8_t *fm1_xip_ptr(uint32_t off) { return host_flash + off; }
static void host_flash_init(void)                      /* a fresh FM-1: erased; a page loads its image after this */
{
    if (!host_flash_inited) {
        host_flash_inited = 1;
        host_flash_erase_all();
    }
}
