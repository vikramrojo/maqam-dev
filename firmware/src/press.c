/* SPDX-License-Identifier: GPL-3.0-only */
/* maqam (MAQAM_PRESS): pressure on a held key bends its notes up, a vibrato under the finger (design D3 of
 * openspec/changes/maqam-aftertouch-bend). fm1_press[k] (fm1_press.h, 0..127; the keys F3..C5) -> every voice of
 * the key's track sounding one of its notes (kb_nt: a chord together; MONO / LEGATO the note that sounds), up to
 * PRESS_CENTS, smoothed (~25 ms). One way only: the reading can only fall below the key's landing, so pressure
 * is never negative; up is a string pressed harder. With AFT ON and MIDI out, each key's pressure goes out as
 * poly key pressure (0xA0) for its note's semitone. Not recorded: a gesture, not a step. Every CTL block,
 * from keyboard_block (the audio context, with the keys' state). AFT OFF: every bend glides back to 0 */
#define PRESS_CENTS 25                 /* full pressure (the lowest range: a vibrato, not a bend to the next note) */
static uint32_t press_age[NTRK][NVOICE];   /* the voice's age when its bend started (a new note: from 0) */
static int32_t press_acc[NTRK][NVOICE];    /* the bend, 1/4096 semitone << 8 */
static uint8_t press_sent[27], press_tick;

static void press_block(void)
{
    uint32_t ti, i, k, j, on = song.g[G_AFT] != 0;
    for (ti = 0; ti < NPART; ti++) {
        track_t *t = &trk[ti];
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &t->v[i];
            int32_t want = 0, *a = &press_acc[ti][i];
            if (!v->active || v->age != press_age[ti][i]) {   /* a new note: no bend carried over */
                press_age[ti][i] = v->age;
                *a = 0;
            }
            if (on && v->active && v->gate)
                for (k = 0; k < FM1_PRESS_NKEY; k++)
                    if (kb_kind[k] == KS_NOTE && kb_trk[k] == ti && fm1_press[k])
                        for (j = 0; j < kb_n[k]; j++)
                            if (kb_nt[k][j] == v->note && fm1_press[k] > want)
                                want = fm1_press[k];
            want = want * PRESS_CENTS * 4096 / (100 * 127) << 8;   /* 25 c = 1024 / 4096 semitone */
            *a += (want - *a) >> 5;                                /* ~32 blocks: ~23 ms */
            press_q[ti][i] = (int16_t)(*a >> 8);
        }
    }
    if (++press_tick < 8u)                                         /* poly pressure out: every ~6 ms */
        return;
    press_tick = 0;
    for (k = 0; k < FM1_PRESS_NKEY; k++) {
        uint32_t pr = on && kb_kind[k] == KS_NOTE ? fm1_press[k] : 0u, mc;
        if (pr == press_sent[k] || (pr && pr + 3u > press_sent[k] && pr < press_sent[k] + 3u))
            continue;                                              /* (moves of less than 3: not sent) */
        press_sent[k] = (uint8_t)pr;
        if (kb_kind[k] != KS_NOTE)
            continue;
        mc = trk_midi_ch(kb_trk[k] % NTRK);
        if (mc > 15u)
            continue;
        for (j = 0; j < kb_n[k]; j++)
            midi_out_event(0x0Au | (0xA0u | mc) << 8 | note_semi(kb_nt[k][j]) << 16 | pr << 24);
    }
}
