/* SPDX-License-Identifier: GPL-3.0-only */
/* The WebAssembly interface of the SLOOP emulator (openspec sloop-web-emulator, design D6): what the page's
 * AudioWorklet calls. One instance = one FM-1: load the flash image (web_flash), web_boot() once, then
 * web_render() from the audio callback. Also compiled natively (tests/hostapp_test.c) as the same C.
 * Freestanding: no imports, no libc beyond firmware/src/libc.c (memmove below). */
#include "sloop_host.c"

#ifdef __wasm__
#define WEB(name) __attribute__((export_name(#name)))
void *memmove(void *d, const void *s, unsigned n)       /* (clang lowers overlapping struct copies to it) */
{
    uint8_t *a = d;
    const uint8_t *b = s;
    if (a < b)
        while (n--)
            *a++ = *b++;
    else
        while (n--)
            a[n] = b[n];
    return d;
}
#else
#define WEB(name)
#endif

#define WEB_MAX 1024u                                   /* frames per web_render */
static float web_l[WEB_MAX], web_r[WEB_MAX];

WEB(web_boot) void web_boot(void) { host_boot(); }
/* n <= 1024 frames into web_out_l() / web_out_r(); the device runs 1 ms at a time until they exist */
WEB(web_render) void web_render(uint32_t n)
{
    host_render(n > WEB_MAX ? WEB_MAX : n, web_l, web_r);
}
WEB(web_out_l) float *web_out_l(void) { return web_l; }
WEB(web_out_r) float *web_out_r(void) { return web_r; }

/* held buttons: logical bit B_* (0 FX .. 13 OCT+), through panel.btn[] to the matrix id */
WEB(web_buttons) void web_buttons(uint32_t mask)
{
    uint32_t b, m = 0;
    for (b = 0; b < NB; b++)
        if ((mask >> b) & 1u)
            m |= 1u << panel.btn[b];
    host_btn_press |= m & ~fm1_in.buttons;
    host_btn_rel |= fm1_in.buttons & ~m;
    fm1_in.buttons = m;
}
WEB(web_keys) void web_keys(uint32_t mask)              /* bit k = key k (0 F3 .. 26 G5) */
{
    mask &= 0x7FFFFFFu;
    host_note_press |= mask & ~fm1_in.notes;
    fm1_in.notes = mask;
}
WEB(web_enc) void web_enc(uint32_t role, int32_t n)     /* roles: 0 SELECT 1 ALGORITHM 2 PRESETS 3-6 KNOB1-4 */
{
    if (role < NE)
        host_enc[panel.enc[role]] += n * panel.dir[role];
}
WEB(web_master) void web_master(uint32_t v) { host_adc_master = v > 1023u ? 1023 : (int32_t)v; }

WEB(web_midi_usb) void web_midi_usb(uint32_t pkt) { midi_in_event(pkt); }   /* a USB-MIDI event packet */
WEB(web_midi_trs) void web_midi_trs(uint32_t b) { um_byte(b & 0xFFu); }     /* a raw byte of the TRS jack */
WEB(web_mo_buf) uint32_t *web_mo_buf(void) { return host_mo; }              /* 256 packets, the ring */
WEB(web_mo_w) uint32_t web_mo_w(void) { return host_mo_w; }

WEB(web_fb) uint16_t *web_fb(void) { return host_fb; }  /* 240x240 RGB565, byte-swapped as the panel takes it */
WEB(web_blits) uint32_t web_blits(void) { return host_blits; }
static uint32_t web_lit(uint64_t ids, int keys)
{
    uint32_t b, m = 0;
    if (keys)
        return (uint32_t)(ids >> 14) & 0x7FFFFFFu;
    for (b = 0; b < NB; b++)
        m |= (uint32_t)((ids >> panel.btn[b]) & 1u) << b;
    return m;
}
static uint64_t web_leds(int glow)                      /* lit: the LEDs on, and the backlight when LIGHTS is up */
{
    uint64_t on = host_led_ids(fm1_led), g = host_led_ids(fm1_led_dim);
    if (fm1_led_bg_ns)
        g |= host_led_ids(fm1_led_bg);
    return glow ? g & ~on : on | g;
}
WEB(web_lit_buttons) uint32_t web_lit_buttons(void) { return web_lit(web_leds(0), 0); }
WEB(web_lit_keys) uint32_t web_lit_keys(void) { return web_lit(web_leds(0), 1); }
/* (extra) the dim ones only: the glow of landmarks and the backlight; web_lit_* has them in it too */
WEB(web_glow_buttons) uint32_t web_glow_buttons(void) { return web_lit(web_leds(1), 0); }
WEB(web_glow_keys) uint32_t web_glow_keys(void) { return web_lit(web_leds(1), 1); }

WEB(web_flash) uint8_t *web_flash(void) { host_flash_init(); return host_flash; }
WEB(web_flash_size) uint32_t web_flash_size(void) { return HOST_FLASH_SIZE; }
WEB(web_flash_dirty) uint32_t *web_flash_dirty(void) { return host_flash_dirty; }   /* 8 x uint32: a bit per 4 KiB sector */
WEB(web_flash_clean) void web_flash_clean(void)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        host_flash_dirty[i] = 0;
}
WEB(web_events) uint32_t web_events(void)               /* bit 0: the UBOOT combo was completed. Cleared by reading */
{
    uint32_t e = host_events;
    host_events = 0;
    return e;
}
WEB(web_version) const char *web_version(void) { return FELUCCA_VERSION; }
WEB(web_ms) uint32_t web_ms(void) { return fm1_ms; }
