/* SPDX-License-Identifier: GPL-3.0-only */
/* maqam (MAQAM_PRESS, openspec/changes/maqam-aftertouch-bend): the pressure arithmetic of fm1_press.h (the landing,
 * the drop below it) and the bend of press.c through the real UI and audio (the ui_pages_test harness): a held key
 * at full pressure bends its voice up 25 cents and back, a chord together, AFT OFF nothing, a new note from 0,
 * poly pressure out, nothing recorded. Built with -DMAQAM_PRESS=1 (tests/run_tests.sh) */
#ifndef MAQAM_PRESS
#define MAQAM_PRESS 1
#endif
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"

static int pfails;
static void pcheck(int ok, const char *what) { printf("press: %-70s %s\n", what, ok ? "ok" : "FAIL"); pfails += !ok; }
static int32_t voice_bend(uint32_t ti, uint32_t note)   /* the bend of the voice gated on note (-1: none) */
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (trk[ti].v[i].active && trk[ti].v[i].gate && trk[ti].v[i].note == note)
            return press_q[ti][i];
    return -1;
}

int main(void)
{
    uint16_t land = 0;
    uint8_t n = 0;
    uint32_t p;
    /* the arithmetic: the lowest of the first two readings lands, then the drop below it, 64 counts = 127 */
    p = fm1_press_calc(&land, &n, 520);
    p |= fm1_press_calc(&land, &n, 500);
    pcheck(p == 0u && land == 500u && n == 2u, "the landing: the lower of the first two driven readings, no pressure yet");
    pcheck(fm1_press_calc(&land, &n, 468) == 64u, "32 counts below the landing: half (64)");
    pcheck(fm1_press_calc(&land, &n, 420) == 127u && fm1_press_calc(&land, &n, 300) == 127u, "64 or more below: full");
    pcheck(fm1_press_calc(&land, &n, 530) == 0u, "above the landing (eased off): 0, never negative");

    panel = PANEL_DEFAULT;
    layers_init();
    host_tracks_init();
    {
        uint32_t i;
        for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    }
    song.sel = 0; go_home(); frame();
    trk[0].p[P_QUANT] = 0; trk[0].p[P_SCALE] = 0; trk[0].p[P_CHORD] = 0; trk[0].p[P_VOICE] = V_POLY; trk[0].p[P_TRANS] = 0; trk[0].p[P_ROOT] = 0; song.octave = 0;
    song.g[G_AFT] = 1;
    fm1_in.notes |= 1u << 11; frames(2);                       /* E4 held */
    pcheck(voice_bend(0, 64) == 0, "E held, no pressure: no bend");
    fm1_press[11] = 127; frames(6);
    pcheck(voice_bend(0, 64) > 1000 && voice_bend(0, 64) <= 1024, "full pressure, AFT ON: E bends up 25 c (1024 / 4096 semitone), smoothed");
    fm1_press[11] = 64; frames(6);
    pcheck(voice_bend(0, 64) > 480 && voice_bend(0, 64) < 560, "half pressure: about 12 c");
    fm1_press[11] = 0; frames(6);
    pcheck(voice_bend(0, 64) >= 0 && voice_bend(0, 64) < 24, "pressure gone: back to the note");
    fm1_in.notes &= ~(1u << 11); frames(2);
    song.g[G_AFT] = 0;
    fm1_in.notes |= 1u << 11; frames(2); fm1_press[11] = 127; frames(6);
    pcheck(voice_bend(0, 64) == 0, "AFT OFF: no bend at full pressure");
    fm1_press[11] = 0; fm1_in.notes &= ~(1u << 11); frames(2);
    song.g[G_AFT] = 1;
    trk[0].p[P_CHORD] = 5;                                    /* POWER: root, fifth, octave on one key */
    fm1_in.notes |= 1u << 11; frames(2); fm1_press[11] = 127; frames(6);
    pcheck(kb_n[11] == 3u && voice_bend(0, kb_nt[11][0]) > 1000 && voice_bend(0, kb_nt[11][1]) > 1000 && voice_bend(0, kb_nt[11][2]) > 1000,
           "a chord key (POWER): all three notes bend together");
    fm1_press[11] = 0; fm1_in.notes &= ~(1u << 11); frames(2);
    trk[0].p[P_CHORD] = 0;
    fm1_in.notes |= 1u << 11; frames(1);
    pcheck(voice_bend(0, 64) >= 0 && voice_bend(0, 64) < 24, "a new note on a voice that was bent: from 0");
    fm1_in.notes |= 1u << 7; frames(2); fm1_press[7] = 127; frames(6);
    pcheck(voice_bend(0, 60) > 1000 && voice_bend(0, 64) < 24, "two keys: each bends on its own pressure (poly)");
    fm1_press[7] = 0; fm1_in.notes = 0; frames(4);
    {   /* MIDI out: poly key pressure (0xA0) for the key's semitone, with AFT ON; none with AFT OFF */
        uint32_t i, seen = 0, val = 0, ch = trk_midi_ch(0), nt;
        usb.config = 1;
        mo_r = mo_w;
        fm1_in.notes |= 1u << 11; frames(2);
        nt = note_semi(kb_nt[11][0]);
        fm1_press[11] = 100; frames(3);
        for (i = mo_r; i != mo_w; i++)
            if ((midi_out_q[i % MQ] & 0xFFFFu) == (0x0Au | (0xA0u | ch) << 8) && ((midi_out_q[i % MQ] >> 16) & 127u) == nt) {
                seen++;
                val = midi_out_q[i % MQ] >> 24;
            }
        pcheck(seen >= 1u && val == 100u, "AFT ON: poly key pressure out for the key's note");
        fm1_press[11] = 0; frames(3);
        for (val = 255u, i = mo_r; i != mo_w; i++)
            if (((midi_out_q[i % MQ] >> 8) & 0xF0u) == 0xA0u)
                val = midi_out_q[i % MQ] >> 24;
        pcheck(val == 0u, "... and 0 when the pressure goes");
        fm1_in.notes = 0; frames(2);
        song.g[G_AFT] = 0;
        mo_r = mo_w;
        fm1_in.notes |= 1u << 11; frames(2); fm1_press[11] = 100; frames(3);
        for (seen = 0, i = mo_r; i != mo_w; i++)
            seen += ((midi_out_q[i % MQ] >> 8) & 0xF0u) == 0xA0u;
        pcheck(seen == 0u, "AFT OFF: no pressure messages (the MIDI stream as before)");
        fm1_press[11] = 0; fm1_in.notes = 0; frames(2);
        song.g[G_AFT] = 1;
        usb.config = 0;
    }
    {   /* recording: the step keeps the plain note */
        uint32_t s;
        steps_clear(&trk[0]);
        song.rec = 1; song.playing = 1;
        fm1_in.notes |= 1u << 11; frames(2); fm1_press[11] = 127; frames(10);
        fm1_press[11] = 0; fm1_in.notes = 0; frames(4);
        song.rec = 0; song.playing = 0; frames(2);
        for (s = 0; s < NSTEP && !trk[0].step[s].n; s++)
            ;
        pcheck(s < NSTEP && trk[0].step[s].note[0] == 64, "recorded with pressure: the step holds E (plain, no lock)");
    }
    song.g[G_AFT] = 0;
    printf("%s\n", pfails ? "PRESS TEST FAILED" : "press test passed");
    return pfails != 0;
}
