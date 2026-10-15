/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP as one host program: the device's own sources (UI, sequencer, engines, FX, storage, panel) in
 * felucca.c's order against hal_host.h, with what main.c and audio.c do around them:
 *   host_boot()        fm1_main's init, minus the hardware
 *   host_run_ms(n)     n ms of device time: the audio half buffers due (audio_block), the main loop's pass
 *   host_render(n,l,r) n stereo frames of float output (the device's 24-bit output / 2^23), time moving as needed
 * Built natively (tests/hostapp_test.c) and as WebAssembly (sloop_web.c), the same C. Not included: main.c,
 * lcd.c, ota.c, editor.c, recovery.c, console.c (hardware, USB serial, update paths). Notes:
 *  - panel_setup (OCT- + OCT+ at power-on) is not run: it blocks on hardware calibration.
 *  - one boot per process / wasm instance: firmware state lives in statics, a reset is a new instance. */
#include "hal_host.h"
#define FELUCCA_OTA 0                    /* no update, no web editor (needs USB) */
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#define FELUCCA_UART 1                   /* TRS MIDI IN: the parser (um_byte) is fed by the page */
#define FELUCCA_FLASH 1
#define FELUCCA_ARRANGER 1
#define __attribute__(x)                 /* (section / aligned: the device linker's) */
#ifndef __wasm__
#undef memset                         /* (a test's <string.h> may have a fortified one) */
#undef memcpy
#undef memcmp
#define memset felucca_memset            /* (hosts have a libc) */
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#endif
#include "felucca_tables.h"
#include "../../firmware/src/libc.c"
#ifndef __wasm__
#undef memset
#undef memcpy
#undef memcmp
#endif
#include "../../firmware/src/gfx.c"
#include "../../firmware/src/core.h"
#include "../../firmware/src/pitch24.h"       /* maqam: 24-tone pitch core */
#include "../../firmware/src/engines.c"
#include "../../firmware/src/drums.c"
#include "../../firmware/src/params.c"
#include "../../firmware/src/voice.c"
#include "../../firmware/src/slicer.c"
#include "../../firmware/src/fx.c"
#include "../../firmware/src/usb.c"
#include "../../firmware/src/midi_uart.c"
#include "../../firmware/src/arranger.c"
#include "../../firmware/src/seq.c"
#include "../../firmware/src/audio.c"            /* audio_block (mix, OUT_SHIFT); its ISR is not called */
#include "../../firmware/src/panel.c"
#include "../../firmware/src/ui.c"
#include "../../firmware/src/ui_song.c"
#include "../../firmware/src/ui_studio.c"
#include "../../firmware/src/icons.c"
#include "../../firmware/src/ui_draw.c"
#include "../../firmware/src/ui_layers.c"
#include "../../firmware/src/ui_menu.c"
#include "../../firmware/src/ui_input.c"
/* the flash backends of felucca.c, on the simulated flash (NOR: erase to 0xFF, programming clears bits) */
static uint8_t flash_ok;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    uint8_t *d = dst;
    uint32_t i;
    host_flash_init();
    if (off > HOST_FLASH_SIZE || n > HOST_FLASH_SIZE - off)
        return -8;
    for (i = 0; i < n; i++)
        d[i] = host_flash[off + i];
    return 0;
}
static int st_erase(uint32_t off)
{
    uint32_t i;
    host_flash_init();
    if (!FL_STORE_OK(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    for (i = 0; i < 0x1000u; i++)
        host_flash[off + i] = 0xFF;
    host_flash_touch(off, 0x1000u);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    host_flash_init();
    if (!FL_STORE_OK(off, n))
        return -8;
    for (i = 0; i < n; i++)
        host_flash[off + i] &= s[i];
    host_flash_touch(off, n);
    return 0;
}
#include "../../firmware/src/storage.c"
#include "../../firmware/src/upreset.c"
#include "../../firmware/src/project.c"
#include "../../firmware/src/splash.c"
#undef __attribute__

/* ---- felucca_init, as main.c has it (power-on: default parts, empty patterns, the project as left) */
static void felucca_init(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        if (i < NPART) {
            set_engine_of(t, TRK_DEF[i][0]);
            apply_preset_to(t, TRK_DEF[i][1]);
            t->engine = t->eng_req;
        }
        track_defaults_steps(t);
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    song.sel = 0;
    song.master_q12 = 2048;
    autosave_resume();
    song.g[G_SYNC] = (int16_t)lights_sync;
    layers_init();
    go_home();
    ui.force = 1;
}
/* felucca_init is main.c's; keep this copy in step with it (tests/hostapp_test.c boots and plays a golden) */

/* ---- the host's clock: audio frames out (44.1 kHz) drive it, a ms at a time */
#define HOST_EV_UBOOT 1u                 /* the UBOOT combo was completed (the device would reset) */
#define HOST_RING 4096u
static float host_ring_l[HOST_RING], host_ring_r[HOST_RING];
static uint32_t host_ring_w, host_ring_rd;       /* frames written / read (counters) */
static uint32_t host_events;
static uint32_t host_mo[256], host_mo_w;         /* MIDI out: the packets the firmware sent (usb.c midi_out_q) */
static uint64_t host_frames;                     /* frames rendered since boot */
static uint32_t host_boot_until, host_frame_ms, host_booted;
static int32_t host_knob = 512 * 16;

static void host_boot(void)
{
    host_flash_init();
    persist_boot();
    settings_init();
    sloop_splash();                              /* (main.c: the logo stays 930 ms) */
    panel_init();
    felucca_init();
    usb.up = 1;                                  /* a USB host is "there": MIDI out is sent (and logged) */
    usb.config = 1;
    host_boot_until = fm1_ms + 930u;
    host_frame_ms = host_boot_until;
    host_booted = 1;
}

static void host_audio_half(void)                /* what fm1_alnk0_irq does for a half buffer */
{
    static int32_t o[2u * HALF_FRAMES];
    uint32_t b, i;
    for (b = 0; b < HALF_FRAMES; b += CTL)
        audio_block(o + 2u * b, CTL);
    for (i = 0; i < HALF_FRAMES; i++) {
        host_ring_l[host_ring_w % HOST_RING] = (float)o[2u * i] * (1.0f / 8388608.0f);   /* 24 bit -> +-1 */
        host_ring_r[host_ring_w % HOST_RING] = (float)o[2u * i + 1u] * (1.0f / 8388608.0f);
        host_ring_w++;
    }
    host_frames += HALF_FRAMES;
}

static void host_main_pass(void)                 /* one pass of fm1_main's loop (battery, MASTER, UBOOT combo, UI) */
{
    int32_t a = fm1_adc_read(FM1_ADC_MASTER), b = fm1_adc_read(FM1_ADC_BATT);
    static uint32_t t0, shown, fired;
    uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
    if (b > 0)
        song.batt_raw = song.batt_raw ? song.batt_raw + (b - song.batt_raw) / 32 : b;
    if (a >= 0) {
        uint32_t k10;
        host_knob += (a * 16 - host_knob) / 8;
        k10 = (uint32_t)(host_knob / 16);
        song.master_q12 = (k10 * k10) >> 8;
    }
    if ((fm1_in.buttons & both) != both || song.playing) {   /* main.c: OCT- + OCT+ held 5 s, stopped */
        if (shown)
            ui_say("UPDATE MODE ", "CANCELLED");
        shown = fired = 0;
        t0 = fm1_ms;
    } else if (fm1_ms - t0 > 2000u && fm1_ms - t0 <= 5000u) {
        uint32_t left = (5000u - (fm1_ms - t0) + 999u) / 1000u;
        if (left != shown) {
            char d[4] = {(char)('0' + left), '.', '.', 0};
            ui_say("UPDATE MODE IN ", d);
            shown = left;
        }
    } else if (fm1_ms - t0 > 5000u && !fired) {
        fired = 1;                               /* the device would stop the audio and enter UBOOT: an event for the page */
        host_events |= HOST_EV_UBOOT;
        ui_say("UBOOT: NOT IN ", "THE EMULATOR");
    }
    if (usb.uboot_req) {                         /* SysEx F0 22 24 35 7D F7 (the page drops it; a test may send it) */
        usb.uboot_req = 0;
        host_events |= HOST_EV_UBOOT;
    }
    felucca_dbg.ui_frames++;
    felucca_dbg.page = ui.page;
    felucca_dbg.home = ui.home;
    ui_input();
    ui_leds();
    ui_draw();
    autosave_tick();
    sections_flush();
}

static void host_run_ms(uint32_t n)
{
    while (n--) {
        fm1_ms++;
        host_ticks += 1000u * FM1_TICKS_PER_US;
        while (host_frames * 1000u < (uint64_t)fm1_ms * FS)      /* the half buffers due by now */
            host_audio_half();
        while (mo_r != mo_w) {                                   /* usb_poll's job: out of the MIDI ring */
            host_mo[host_mo_w++ & 255u] = midi_out_q[mo_r % MQ];
            mo_r++;
        }
        if (!host_booted || (int32_t)(fm1_ms - host_boot_until) < 0)
            continue;                                            /* the logo; the audio runs */
        if (host_boot_until) {
            lcd_fill(0, 0, 240, 240, C_BLACK);
            host_boot_until = 0;
        }
        if (fm1_ms - host_frame_ms >= 16u) {                     /* ~60 UI frames/s (main.c: 15 ms + the draw) */
            host_frame_ms = fm1_ms;
            host_main_pass();
        } else {
            ui_input();                                          /* (main.c polls it while it waits) */
        }
    }
}

/* n frames of output (n <= HOST_RING / 2) into l, r; the device runs for as long as that takes */
static void host_render(uint32_t n, float *l, float *r)
{
    uint32_t i;
    while (host_ring_w - host_ring_rd < n)
        host_run_ms(1);
    if (host_ring_w - host_ring_rd > HOST_RING)               /* (nobody read for a while: the oldest are gone) */
        host_ring_rd = host_ring_w - HOST_RING;
    for (i = 0; i < n; i++, host_ring_rd++) {
        l[i] = host_ring_l[host_ring_rd % HOST_RING];
        r[i] = host_ring_r[host_ring_rd % HOST_RING];
    }
}
