/* SPDX-License-Identifier: GPL-3.0-only */
/* The SLOOP host app (web/emu/sloop_host.c, through the exports of sloop_web.c the page uses) as a whole:
 * boot from blank flash, a drum pattern entered with the panel (SELECT, SEQ + keys), PLAY with a USB note
 * on channel 1 and a TRS note pair with running status, four bars rendered and hashed against
 * tests/golden_hostapp.txt (web/test_web.mjs runs the same scenario on build/emu/sloop.wasm and must get
 * the same hashes). Then: save, power off (the process starts again on the same flash: the firmware is
 * statics, a boot is a new process) and the project is the same, and plays the second golden. MIDI clock
 * with SYNC = USB sets the tempo, SYNC = TRS ignores USB, and OCT- + OCT+ held 5 s does not enter UBOOT.
 *   build/host/hostapp_test tests/golden_hostapp.txt [build/host]
 *   GOLDEN_UPDATE=1 rewrites the golden file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../web/emu/sloop_web.c"

static int fails;
static void check(int ok, const char *what) { printf("hostapp: %-70s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

/* ---- the scenario, call for call what test_web.mjs does: run(ms) renders 128-frame chunks and hashes them */
static uint32_t hash;
static int32_t peak;
static void run(uint32_t ms)
{
    uint32_t left = ms * 441u / 10u, n, i;
    while (left) {
        n = left < 128u ? left : 128u;
        web_render(n);
        for (i = 0; i < n; i++) {
            int32_t m = (int32_t)(web_l[i] * 8388608.0f);
            peak = m > peak ? m : -m > peak ? -m : peak;
            hash = (hash ^ (uint32_t)(int32_t)(web_l[i] * 8388608.0f)) * 16777619u;
            hash = (hash ^ (uint32_t)(int32_t)(web_r[i] * 8388608.0f)) * 16777619u;
        }
        left -= n;
    }
}
static void keytap(uint32_t k) { web_keys(1u << k); run(60); web_keys(0); run(60); }
static void btntap(uint32_t b) { web_buttons(1u << b); run(60); web_buttons(0); run(60); }
static uint32_t on(uint32_t ch, uint32_t note, uint32_t vel) { return 0x09u | (0x90u | ch) << 8 | note << 16 | vel << 24; }
static uint32_t off(uint32_t ch, uint32_t note) { return 0x08u | (0x80u | ch) << 8 | note << 16; }
static uint32_t active(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active != 0;
    return n;
}
static int any_hit(uint32_t i)
{
    uint32_t l;
    for (l = 0; l < 16u; l++)
        if (dstep_has(&TDRUM->dstep[i], l))
            return 1;
    return 0;
}
static void play_four_bars(void)                 /* PLAY, a held USB note (part 1) and a TRS pair (part 2) */
{
    btntap(B_PLAY);
    web_midi_usb(on(0, 48, 100));
    web_midi_trs(0x91); web_midi_trs(52); web_midi_trs(100); web_midi_trs(55); web_midi_trs(100);   /* running status */
    run(10667);                                  /* 16 beats at 90 BPM */
}
static void scenario1(void)
{
    uint32_t k;
    hash = 2166136261u;
    web_boot();
    run(1200);
    for (k = 0; k < 3u; k++) {                   /* ALGORITHM: the next track, a step a turn */
        web_enc(1, 1);
        run(40);
    }
    check(song.sel == TRK_DRUM, "ALGORITHM +3 times selects the drum track");
    web_buttons(1u << B_SEQ);                    /* SEQ held: the white keys are steps */
    run(200);
    keytap(0); keytap(7); keytap(14);
    web_buttons(0);
    run(100);
    check(any_hit(0) && any_hit(4) && any_hit(8) && !any_hit(1), "SEQ + keys 1, 8, 15: drum steps 1, 5, 9");
    btntap(B_PLAY);
    check(song.playing, "PLAY: the sequencer runs");
    btntap(B_PLAY);
    check(!song.playing, "PLAY again: stopped");
    play_four_bars();
    check(song.playing && active(&trk[0]) == 1 && active(&trk[1]) == 2,
          "USB note ch 1 and TRS running status ch 2: 1 + 2 voices");
}
static void scenario1_end(void)
{
    web_midi_usb(off(0, 48));
    web_midi_trs(0x81); web_midi_trs(52); web_midi_trs(0); web_midi_trs(55); web_midi_trs(0);   /* (a note-on with velocity 0 would do too) */
    run(500);
    btntap(B_PLAY);
    check(!song.playing && active(&trk[0]) == 0, "released and stopped: no voice left");
}
static void scenario2(void)                      /* after the reboot: the same four bars, the pattern from the flash */
{
    hash = 2166136261u;
    web_boot();
    run(1200);
    check(any_hit(0) && any_hit(4) && any_hit(8) && !any_hit(1), "the drum pattern came back from the flash");
    play_four_bars();
}

/* ---- golden file: "scenario HEX" / "reboot HEX" lines (GOLDEN_UPDATE=1 sets the one just rendered) */
static const char *golden_file;
static int golden(const char *name, uint32_t h)
{
    char line[96], key[16], text[512] = "";
    uint32_t v, s = 0, r = 0, have = 0, want = 0;
    FILE *f = fopen(golden_file, "r");
    while (f && fgets(line, sizeof line, f))
        if (sscanf(line, "%15s %x", key, &v) == 2) {
            if (!strcmp(key, "scenario")) s = v;
            if (!strcmp(key, "reboot")) r = v;
            if (!strcmp(key, name)) { have = 1; want = v; }
        }
    if (f)
        fclose(f);
    printf("hostapp: %s hash %08x (golden %s)\n", name, h, have ? (want == h ? "same" : "DIFFERENT") : "none");
    if (getenv("GOLDEN_UPDATE")) {
        if (!strcmp(name, "scenario")) s = h; else r = h;
        snprintf(text, sizeof text, "# tests/hostapp_test.c and web/test_web.mjs: hash of the rendered output (the 24-bit ints)\nscenario %08x\nreboot %08x\n", s, r);
        f = fopen(golden_file, "w");
        fputs(text, f);
        fclose(f);
        return 1;
    }
    return have && want == h;
}

static const char *flash_file = "build/host/hostapp.flash";

static int phase2(void)
{
    FILE *f = fopen(flash_file, "rb");
    uint32_t sum = 0;
    project_t p;
    if (!f || fread(web_flash(), 1, web_flash_size(), f) != web_flash_size() || fread(&sum, 4, 1, f) != 1) {
        puts("hostapp: no flash image");
        return 1;
    }
    fclose(f);
    scenario2();
    proj_capture(&p);
    check(p.sum == sum, "reboot from the same flash: the working project is identical");
    check(golden("reboot", hash), "reboot render matches the golden hash");
    return fails != 0;
}

int main(int argc, char **argv)
{
    uint32_t sum, i;
    FILE *f;
    golden_file = argc > 1 ? argv[1] : "tests/golden_hostapp.txt";
    if (argc > 3 && !strcmp(argv[3], "phase2")) {
        flash_file = argv[2];
        return phase2();
    }
    if (argc > 2) {
        static char path[512];
        snprintf(path, sizeof path, "%s/hostapp.flash", argv[2]);
        flash_file = path;
    }
    /* ---- 1. blank flash: boot, enter, play */
    scenario1();
    check(peak > 100000 && peak < 4194304, "the render has sound, below the -6 dBFS ceiling");
    check(golden("scenario", hash), "four bars match the golden hash");
    {   /* the version and the screen */
        uint32_t px = 0;
        for (i = 0; i < 240u * 240u; i++)
            px += web_fb()[i] != 0;
        check(px > 1000 && web_blits() > 10, "the screen was drawn");
        check(!strcmp(web_version(), FELUCCA_VERSION), "web_version");
    }
    scenario1_end();
    {   /* ---- 2. save, and power off */
        project_t p, q;
        proj_capture(&p);
        project_save(0);
        run(100);
        proj_capture(&q);
        check(q.sum == p.sum, "capture is stable");
        memset(&proj_slot[0], 0, sizeof proj_slot[0]);
        project_load(0);                         /* from the flash, not RAM */
        proj_capture(&q);
        check(q.sum == p.sum, "SAVE slot 1, load it back from flash: identical project");
        run(25000);                              /* idle: the autosave (20 s gap) writes the working project */
        proj_capture(&q);
        sum = q.sum;
        {
            uint32_t n = 0;
            for (i = 0; i < 8u; i++)
                n += (uint32_t)__builtin_popcount(web_flash_dirty()[i]);
            printf("hostapp: %u flash sectors written\n", n);
            check(n >= 2, "flash sectors written (project, autosave)");
        }
        f = fopen(flash_file, "wb");
        if (!f) { puts("cannot write the flash image"); return 1; }
        fwrite(web_flash(), 1, web_flash_size(), f);
        fwrite(&sum, 4, 1, f);
        fclose(f);
    }
    /* ---- 3. MIDI clock, SYNC = USB: the tempo follows; SYNC = TRS: USB is ignored */
    song.g[G_SYNC] = 1;
    lights_sync = 1;
    song.g[G_BPM] = 90;
    web_midi_usb(0x0Fu | 0xFAu << 8);
    for (i = 0; i < 24u * 3u; i++) {             /* 100 BPM: a pulse every 25 ms */
        web_midi_usb(0x0Fu | 0xF8u << 8);
        run(25);
    }
    check(song.g[G_BPM] == 100, "SYNC = USB, clock at 100 BPM: tempo 100");
    check(song.playing, "SYNC = USB: START starts the sequencer");
    web_midi_usb(0x0Fu | 0xFCu << 8);
    run(100);
    check(!song.playing, "SYNC = USB: STOP stops");
    song.g[G_SYNC] = 2;
    lights_sync = 2;
    for (i = 0; i < 24u * 3u; i++) {
        web_midi_usb(0x0Fu | 0xF8u << 8);        /* 120 BPM on the wrong route: ignored */
        run(20);
    }
    check(song.g[G_BPM] == 100, "SYNC = TRS, clock on the USB route: ignored");
    for (i = 0; i < 24u * 3u; i++) {
        web_midi_trs(0xF8);                      /* and on the TRS route (realtime between data bytes too) */
        run(20);
    }
        check(song.g[G_BPM] >= 123 && song.g[G_BPM] <= 127, "SYNC = TRS, TRS clock at 125 BPM: tempo follows (+-2)");
    song.g[G_SYNC] = 0;
    lights_sync = 0;
    run(1000);
    /* ---- 4. OCT- + OCT+ for 5 s, stopped: the UBOOT countdown, then an event, no UBOOT */
    (void)web_events();
    web_buttons(1u << B_OCTDN | 1u << B_OCTUP);
    run(2600);
    check(web_events() == 0, "OCT- + OCT+ held 2.6 s: nothing yet");
    run(2800);
    check(web_events() == HOST_EV_UBOOT, "OCT- + OCT+ held 5 s: the UBOOT event bit (the page shows a toast)");
    check(web_events() == 0 && flash_ok && usb.config, "the event is read once; the firmware keeps running");
    web_buttons(0);
    run(100);
    {   /* held while playing: nothing (the drum track's ghost / hard modifiers) */
        song.playing = 1;
        web_buttons(1u << B_OCTDN | 1u << B_OCTUP);
        run(6000);
        web_buttons(0);
        song.playing = 0;
        check(web_events() == 0, "OCT- + OCT+ held 6 s while playing: no event");
    }
    /* SysEx F0 22 24 35 7D F7 (UBOOT) through the USB route: the firmware sees it as the device would */
    web_midi_usb(0x04u | 0xF0u << 8 | 0x22u << 16 | 0x24u << 24);
    web_midi_usb(0x06u | 0x35u << 8 | 0x7Du << 16 | 0xF7u << 24 | 0u);
    check(1, "(SysEx is dropped by the page, never sent here)");
    /* ---- the next boot: a new process on the saved flash */
    {
        char cmd[3][600];
        pid_t pid;
        int st = 0;
        snprintf(cmd[0], sizeof cmd[0], "%s", argv[0]);
        snprintf(cmd[1], sizeof cmd[1], "%s", golden_file);
        snprintf(cmd[2], sizeof cmd[2], "%s", flash_file);
        pid = fork();
        if (pid == 0) {
            execl(cmd[0], cmd[0], cmd[1], cmd[2], "phase2", (char *)0);
            _exit(127);
        }
        waitpid(pid, &st, 0);
        check(WIFEXITED(st) && WEXITSTATUS(st) == 0, "second boot (new process, the saved flash)");
    }
    printf(fails ? "hostapp: FAILED (%d)\n" : "hostapp: all ok\n", fails);
    return fails != 0;
}
