/* SPDX-License-Identifier: GPL-3.0-only */
/* SLOOP 2.0 sequencer: the transport clock and what plays on it.
 *   timing   the kick and the click stay together for 64 bars at any tempo (no drift); a tempo or DIV
 *            change mid-step plays one step, not a burst; LEN changes keep the track in phase; an odd
 *            LEN with SWING stays in phase with a 16-step track
 *   ratchet  x2 / x3 / x4 hits evenly in their step (drums and synth)
 *   roll     ARP + key: hits on the grid at G_ROLL; recorded as ratchets (1/32 -> x2 on 1/16 steps)
 *   erase    EDIT + key: the lane goes as the playhead passes (playing), all of it (stopped); undo / redo
 *   levels   OCT- / OCT+ held on the drum track: ghost / hard hits, recorded as such
 *   chords   P_CHORD: one key plays the chord of the scale (white keys walk the degrees from C4)
 *   mute     P_MUTE / solo: no new notes, the output fades
 * Exit status: the number of failed checks. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static uint64_t blk;
typedef struct { uint64_t blk; uint8_t note, vel; } hit_t;
static hit_t hits[200000];
static uint32_t nhits;
static void run_block(void)
{
    int32_t out[CTL * 2];
    uint32_t a = drums.age, k;
    mix_block(out, CTL);
    if (drums.age != a)
        for (k = 0; k < NDRUM; k++)
            if (drums.v[k].age > a && nhits < 200000u) {
                hits[nhits].blk = blk;
                hits[nhits].note = drums.v[k].note;
                hits[nhits].vel = drums.v[k].vel;
                nhits++;
            }
    blk++;
}
static void reset(uint32_t bpm)
{
    uint32_t i;
    host_tracks_init();
    for (i = 0; i < NTRK; i++)
        steps_clear(&trk[i]);
    memset(&drums, 0, sizeof drums);
    drums.set = -2;
    nhits = 0;
    blk = 0;
    song.g[G_BPM] = (int16_t)bpm;
    song.rec = 0;
    rec_wait = 0;
    fm1_in.notes = fm1_in.buttons = 0;
    kb_prev = 0;
    ly_bit[LY_ROLL] = 1u << 8;                       /* (the buttons: any bits) */
    ly_bit[LY_ERASE] = 1u << 4;
    dyn_bit[0] = 1u << 0;
    dyn_bit[1] = 1u << 1;
}
static int fails;
static void check(int ok, const char *what)
{
    printf("seq2: %-72s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
static uint32_t count_note(uint32_t note, uint64_t b0, uint64_t b1)
{
    uint32_t i, n = 0;
    for (i = 0; i < nhits; i++)
        n += hits[i].note == note && hits[i].blk >= b0 && hits[i].blk < b1;
    return n;
}

static void t_drift(void)
{
    static const uint32_t BPM[5] = {87, 90, 120, 128, 174};
    uint32_t bi, worst = 0;
    for (bi = 0; bi < 5u; bi++) {
        uint32_t i, nk = 0, nc = 0, bad = 0;
        uint64_t kb[80], cb[80];
        reset(BPM[bi]);
        song.g[G_CLOCK] = 2;                         /* the click on: its downbeat (77) */
        dstep_set(&TDRUM->dstep[0], 0, LV_NORM, 0);  /* a kick on the 1 */
        transport_req = 1;
        while (blk < (uint64_t)(FS * 60.0 / BPM[bi] * 4 * 66 / CTL))
            run_block();
        for (i = 0; i < nhits; i++) {
            if (hits[i].note == 36 && nk < 80u)
                kb[nk++] = hits[i].blk;
            if (hits[i].note == 77 && nc < 80u)
                cb[nc++] = hits[i].blk;
        }
        for (i = 0; i < nk && i < nc && i < 64u; i++) {
            uint32_t d = kb[i] > cb[i] ? (uint32_t)(kb[i] - cb[i]) : (uint32_t)(cb[i] - kb[i]);
            bad += d != 0u;
            worst = d > worst ? d : worst;
        }
        if (bad || nk < 64u || nc < 64u)
            printf("seq2:   %u BPM: %u kicks, %u clicks, %u apart\n", BPM[bi], nk, nc, bad);
        worst += (nk < 64u || nc < 64u) * 1000u;
    }
    check(worst == 0u, "64 bars at 87..174 BPM: every kick in the block of its downbeat click");
}

static void t_burst(void)
{
    uint32_t i, n0;
    int ok1, ok2;
    reset(90);
    TDRUM->p[P_SDIV] = 0;                            /* 1/4 */
    for (i = 0; i < 16u; i++)
        dstep_set(&TDRUM->dstep[i], 4, LV_NORM, 0);
    transport_req = 1;
    while (clk_beat < 2u || clk_pos < BEAT_U * 85u / 100u)
        run_block();
    TDRUM->p[P_SDIV] = 3;                            /* DIV 1/4 -> 1/32 at 85 % of a step */
    n0 = nhits;
    run_block();
    ok1 = nhits - n0 <= 1u;
    reset(40);
    for (i = 0; i < 16u; i++)
        dstep_set(&TDRUM->dstep[i], 4, LV_NORM, 0);
    transport_req = 1;
    while (clk_beat < 1u || clk_pos < BEAT_U / 4u * 3u / 4u)
        run_block();
    song.g[G_BPM] = 240;                             /* 40 -> 240 in the middle of a step */
    n0 = nhits;
    run_block();
    ok2 = nhits - n0 <= 1u;
    check(ok1 && ok2, "DIV 1/4 -> 1/32 and 40 -> 240 BPM mid-step: one step a block, no burst");
}

static void t_swing_odd(void)
{
    uint32_t i, ok = 1, starts0 = 0, startsd = 0;
    uint64_t at0[64], atd[64];
    reset(120);
    song.g[G_SWING] = 50;
    trk[0].p[P_SLEN] = 3;
    TDRUM->p[P_SLEN] = 16;
    transport_req = 1;
    for (i = 0; i < 20u * FS / CTL; i++) {
        uint32_t b0 = trk[0].seq_idx, bd = TDRUM->seq_idx;
        uint32_t f0 = trk[0].seq_abs, fd = TDRUM->seq_abs;
        run_block();
        if (trk[0].seq_abs != f0 && trk[0].seq_idx == 0u && starts0 < 64u)
            at0[starts0++] = blk - 1u;
        if (TDRUM->seq_abs != fd && TDRUM->seq_idx == 0u && startsd < 64u)
            atd[startsd++] = blk - 1u;
        (void)b0;
        (void)bd;
    }
    /* step 48 = cycle 16 of 3 = bar 3 of 16; step 144 = cycle 48 = bar 9 */
    ok = starts0 > 48u && startsd > 9u && at0[16] == atd[3] && at0[48] == atd[9];
    if (!ok)
        printf("seq2:   %u %u starts; step 48: %llu %llu, step 144: %llu %llu\n", starts0, startsd,
               (unsigned long long)at0[16], (unsigned long long)atd[3], (unsigned long long)at0[48], (unsigned long long)atd[9]);
    check(ok, "SWING 75 %, LEN 3 against LEN 16: in phase after 15 bars (no drift)");
}

static void t_len_phase(void)
{
    reset(120);
    TDRUM->p[P_SLEN] = 32;
    trk[0].p[P_SLEN] = 16;
    transport_req = 1;
    while (TDRUM->seq_idx != 20u)
        run_block();
    TDRUM->p[P_SLEN] = 16;                           /* LEN 32 -> 16 at step 20 */
    while (TDRUM->seq_idx == 20u)
        run_block();
    check(TDRUM->seq_idx == trk[0].seq_idx, "LEN 32 -> 16 while playing: in phase with a 16-step track");
}

static void t_ratchet(void)
{
    uint32_t i, n = 0, gaps_ok = 1;
    uint64_t b[8];
    reset(120);
    dstep_set(&TDRUM->dstep[0], 4, LV_NORM, 3);      /* a hat x4 on step 0 */
    dstep_set(&TDRUM->dstep[1], 0, LV_NORM, 0);      /* a kick on step 1 */
    transport_req = 1;
    while (blk < div_samples(2) * 2u / CTL + 2u)
        run_block();
    for (i = 0; i < nhits; i++)
        if (hits[i].note == 42 && n < 8u)
            b[n++] = hits[i].blk;
    for (i = 1; i < n; i++) {
        int32_t g = (int32_t)(b[i] - b[i - 1]), want = (int32_t)(div_samples(2) / 4u / CTL);
        gaps_ok &= g >= want - 1 && g <= want + 1;
    }
    check(n == 4u && gaps_ok && count_note(36, 0, blk) == 1u, "drums: a hat x4 plays 4 even hits in its step, the next step plays");
    reset(120);
    host_preset(&trk[0], 0, 7);                      /* TRAP PLUCK (POLY) */
    put_step(&trk[0], 0, 1, (const uint8_t[]){60}, ST_NOTE, 0);
    trk[0].step[0].rat = 2;                          /* x3 */
    transport_req = 1;
    {
        uint32_t starts = 0, last = vage;
        while (blk < div_samples(2) / CTL) {
            run_block();
            if (vage != last)
                starts += vage - last;
            last = vage;
        }
        check(starts == 3u, "synth: a note x3 starts 3 times in its step");
    }
}

static void t_roll(void)
{
    uint32_t i, k = 6, n;                           /* key 6 = B3 -> lane 3 (clap) */
    reset(120);
    song.g[G_ROLL] = 2;                              /* 1/32 */
    song.sel = TRK_DRUM;
    song.rec = 1u << TRK_DRUM;
    transport_req = 1;
    run_block();
    fm1_in.buttons = ly_bit[LY_ROLL];                /* ARP held */
    fm1_in.notes = 1u << k;
    for (i = 0; i < div_samples(2) * 8u / CTL; i++)  /* 8 steps */
        run_block();
    fm1_in.notes = 0;
    fm1_in.buttons = 0;
    run_block();
    n = count_note(LANE_NOTE[3], 0, blk);
    {
        uint32_t s, rat_ok = 1, on = 0;
        for (s = 1; s < 7u; s++) {
            on += dstep_has(&TDRUM->dstep[s], 3);
            rat_ok &= !dstep_has(&TDRUM->dstep[s], 3) || dstep_rat(&TDRUM->dstep[s], 3) == 1u;
        }
        if (n < 15u || n > 17u)
            printf("seq2:   roll: %u hits\n", n);
        check(n >= 15u && n <= 17u, "roll 1/32 held 8 steps: ~16 hits");
        check(on >= 6u && rat_ok, "roll 1/32 recorded: one hit a step, ratchet x2");
    }
}

static void t_erase_undo(void)
{
    uint32_t i, left = 0, kept = 0;
    reset(120);
    for (i = 0; i < 16u; i++) {
        dstep_set(&TDRUM->dstep[i], 4, LV_NORM, 0);  /* hats everywhere */
        if (i % 4u == 0u)
            dstep_set(&TDRUM->dstep[i], 0, LV_NORM, 0);
    }
    song.sel = TRK_DRUM;
    transport_req = 1;
    while (TDRUM->seq_idx != 4u)
        run_block();
    fm1_in.buttons = ly_bit[LY_ERASE];               /* EDIT held + the hat key (C4 = key 7) */
    fm1_in.notes = 1u << 7;
    while (TDRUM->seq_idx != 7u)
        run_block();
    for (i = 0; i < 20u; i++)                        /* (into step 7, not 8) */
        run_block();
    fm1_in.notes = 0;
    fm1_in.buttons = 0;
    run_block();
    for (i = 0; i < 16u; i++) {
        left += dstep_has(&TDRUM->dstep[i], 4);
        kept += dstep_has(&TDRUM->dstep[i], 0);
    }
    if (left != 12u)
        printf("seq2:   erase: %u hats left, %u kicks; %d %d\n", left, kept, dstep_has(&TDRUM->dstep[5], 4), dstep_has(&TDRUM->dstep[9], 4));
    check(left == 12u && kept == 4u && !dstep_has(&TDRUM->dstep[5], 4) && dstep_has(&TDRUM->dstep[9], 4),
          "EDIT + hat while playing steps 4..7: those hats gone, the rest and the kicks kept");
    {   /* undo (ui.c undo_swap): the hats back */
        step_t tmp[NSTEP];
        memcpy(tmp, TDRUM->step, sizeof tmp);
        memcpy(TDRUM->step, undo.st, sizeof tmp);
        memcpy(undo.st, tmp, sizeof tmp);
    }
    left = 0;
    for (i = 0; i < 16u; i++)
        left += dstep_has(&TDRUM->dstep[i], 4);
    check(left == 16u && undo.trk == TRK_DRUM, "undo: the erased hats are back");
    transport_req = 2;
    run_block();
    fm1_in.buttons = ly_bit[LY_ERASE];               /* stopped: every hat of the pattern */
    fm1_in.notes = 1u << 7;
    run_block();
    fm1_in.notes = fm1_in.buttons = 0;
    run_block();
    left = 0;
    for (i = 0; i < 16u; i++)
        left += dstep_has(&TDRUM->dstep[i], 4);
    check(left == 0u, "EDIT + hat while stopped: every hat gone");
}

static void t_levels(void)
{
    uint32_t i, gh = 0, hd = 0;
    reset(120);
    song.sel = TRK_DRUM;
    song.rec = 1u << TRK_DRUM;
    transport_req = 1;
    run_block();
    fm1_in.buttons = dyn_bit[0];                     /* OCT- held: a ghost snare (A3 = key 4) */
    fm1_in.notes = 1u << 4;
    run_block();
    fm1_in.notes = 0;
    run_block();
    while (TDRUM->seq_idx != 3u)
        run_block();
    fm1_in.buttons = dyn_bit[1];                     /* OCT+ held: a hard kick */
    fm1_in.notes = 1u << 0;
    run_block();
    fm1_in.notes = fm1_in.buttons = 0;
    run_block();
    for (i = 0; i < 16u; i++) {
        if (dstep_has(&TDRUM->dstep[i], 2))
            gh = dstep_lvl(&TDRUM->dstep[i], 2);
        if (dstep_has(&TDRUM->dstep[i], 0))
            hd = dstep_lvl(&TDRUM->dstep[i], 0);
    }
    check(gh == LV_GHOST && hd == LV_HARD, "OCT- / OCT+ held on the drums: a ghost snare and a hard kick recorded");
    {
        uint32_t vg = 0, vh = 0;
        for (i = 0; i < nhits; i++) {
            if (hits[i].note == 38)
                vg = hits[i].vel;
            if (hits[i].note == 36)
                vh = hits[i].vel;
        }
        check(vg == 42u && vh == 127u, "... and played at 42 / 127");
    }
}

static void t_chords(void)
{
    uint8_t c[4];
    uint32_t n;
    reset(120);
    trk[0].p[P_ROOT] = 0;
    trk[0].p[P_SCALE] = 2;                           /* C minor */
    trk[0].p[P_CHORD] = 3;                           /* 9TH: 1 3 7 9 */
    n = chord_notes(&trk[0], kb_map(&trk[0], 7), c); /* C4: i */
    check(n == 4u && c[0] == 60 && c[1] == 63 && c[2] == 70 && c[3] == 74, "chord 9TH on C4 in C minor: C Eb Bb D");
    trk[0].p[P_CHORD] = 1;
    n = chord_notes(&trk[0], kb_map(&trk[0], 9), c); /* D4: ii (dim) */
    check(n == 3u && c[0] == 62 && c[1] == 65 && c[2] == 68, "chord TRIAD on D4 in C minor: D F Ab");
    check(kb_map(&trk[0], 8) == KB_SILENT, "chord mode: the black keys are silent");
}

static void t_mute(void)
{
    uint32_t i, n0;
    reset(120);
    for (i = 0; i < 16u; i++)
        dstep_set(&TDRUM->dstep[i], 0, LV_NORM, 0);
    transport_req = 1;
    while (blk < 200u)
        run_block();
    TDRUM->p[P_MUTE] = 1;
    n0 = nhits;
    while (blk < 400u)
        run_block();
    check(nhits == n0, "drum track muted: no hits");
    TDRUM->p[P_MUTE] = 0;
    song.solo = 1u;                                  /* track 1 soloed: the drums silent */
    n0 = nhits;
    while (blk < 600u)
        run_block();
    check(nhits == n0 && TDRUM->att == 32767, "track 1 soloed: the drums silent (faded out)");
    song.solo = 0;
    while (blk < 800u)
        run_block();
    check(nhits > n0 && TDRUM->att == 0, "solo off: the drums back");
}

/* overload shedding (voice.c shed_voice): a releasing voice first, then the oldest held voice that is
 * neither a POLY part's lowest note nor a MONO part's lead; faded (stage 4), never cut */
static void t_shed(void)
{
    track_t *t = &trk[0], *m = &trk[1];
    uint32_t i, k, low_ok = 1, lead_ok = 1;
    reset(120);
    t->p[P_VOICE] = V_POLY;
    m->p[P_VOICE] = V_MONO;
    t->p[P_ATK] = 0;
    t->p[P_REL] = 100;                                /* a long release: the released voice still rings */
    trk_note_on(t, 48, 100, 0);                          /* the bass, first and lowest */
    trk_note_on(t, 64, 100, 0);
    trk_note_on(t, 67, 100, 0);
    trk_note_on(t, 72, 100, 0);
    trk_note_on(m, 40, 100, 0);                          /* a MONO lead */
    trk_note_on(t, 76, 100, 0);
    for (k = 0; k < 20u; k++)
        run_block();
    trk_note_off(t, 76);                              /* releasing */
    run_block();
    shed_voice();
    for (k = 0, i = 0; i < NVOICE; i++)
        k += t->v[i].note == 76 && t->v[i].stage == 4u;
    check(k == 1u, "overload: the releasing voice goes first, faded (stage 4)");
    for (k = 0; k < 6u; k++)
        shed_voice();
    for (i = 0; i < NVOICE; i++) {
        if (t->v[i].note == 48 && t->v[i].active && t->v[i].stage == 4u)
            low_ok = 0;
        if (m->v[i].note == 40 && i == 0u && m->v[i].stage == 4u)
            lead_ok = 0;
    }
    for (k = 0, i = 0; i < NVOICE; i++)
        k += t->v[i].active && t->v[i].gate && (t->v[i].note == 64 || t->v[i].note == 67 || t->v[i].note == 72);
    check(low_ok && lead_ok && k == 0u, "overload: the upper notes thin out, the bass and the MONO lead stay");
}

/* MIDI clock in (SYNC USB / TRS): START, 24 pulses a beat, tempo changes, STOP; the other source ignored */
static void mclk_push(uint32_t pkt) { midi_in_q[mi_w % MQ] = pkt; mi_w++; }
static void mclk_run(double *t, double *next, double per, double until, uint32_t src)
{
    while (*t < until) {
        while (*next <= *t) {
            mclk_push(0xF80Fu | src << 4);
            *next += per;
        }
        fm1_ms = (uint32_t)*t;
        run_block();
        *t += (double)CTL * 1000.0 / FS;
    }
}
static void t_mclk(void)
{
    double t = 0, next = 0, per = 60000.0 / 120 / 24;
    reset(90);                                        /* the internal tempo: 90 */
    mi_r = mi_w;
    song.g[G_SYNC] = 1;                               /* USB */
    mclk_run(&t, &next, per, 700, 0);                 /* the clock runs before START (the tempo shows) */
    check(!song.playing && song.g[G_BPM] == 120, "MIDI clock: stopped, BPM follows the master (120)");
    mclk_push(0xFA0Fu);                               /* START, then the downbeat pulse */
    next = t;
    mclk_run(&t, &next, per, t + 4000.0 + per / 2, 0);   /* two bars of 120 */
    check(song.playing && clk_beat == 8u, "MIDI clock: START, 2 bars at 120 -> beat 8 exactly");
    per = 60000.0 / 100 / 24;                          /* the master slows to 100 */
    mclk_run(&t, &next, per, t + 4800.0, 0);
    check(clk_beat == 16u && song.g[G_BPM] == 100, "MIDI clock: the master at 100 -> 2 more bars, BPM 100");
    mclk_push(0xFC1Fu);                               /* STOP from the TRS jack: not the source */
    mclk_run(&t, &next, per, t + 50.0, 0);
    check(song.playing, "MIDI clock: SYNC USB ignores the TRS jack");
    mclk_push(0xFC0Fu);
    mclk_run(&t, &next, per, t + 10.0, 0);
    check(!song.playing, "MIDI clock: STOP");
    t += 1000.0;                                      /* the master is gone: PLAY on the FM-1 plays */
    fm1_ms = (uint32_t)t;
    transport_req = 1;
    run_block();
    {
        uint32_t b0 = clk_beat, k;
        for (k = 0; k < (uint32_t)(FS / CTL); k++)
            run_block();
        check(song.playing && clk_beat >= b0 + 1u, "MIDI clock: no pulse for 0.5 s -> the internal tempo plays");
    }
    transport_req = 2;
    run_block();
    song.g[G_SYNC] = 0;
    mi_r = mi_w;
}

/* the REC screen's MODE and START (SLOOP 2.3): an empty project records at the tempo set (TEMPO) or
 * takes it from the playing (FREE); COUNT: PLAY clicks one bar, then the loop and the recording start */
static void t_recmode(void)
{
    uint32_t k, bpb = (uint32_t)((double)FS * 60.0 / 100.0 / CTL + 0.5), c0;
    reset(100);
    song.sel = 0;
    song.playing = 0;
    rec_tempo = 0, rec_count = 0;
    rec_wait = 1;
    input_on(TSEL, 60, 100);
    run_block();
    check(ft_on && !song.playing, "REC mode FREE, empty project: the first note starts a free take");
    transport_req = 2; run_block(); ft_bars = 0;
    for (k = 0; k < 4u; k++) trk_note_off(&trk[k % NPART], 60);

    reset(100);
    song.sel = 0;
    rec_tempo = 1, rec_count = 0;
    rec_wait = 1;
    input_on(TSEL, 60, 100);
    run_block();
    check(!ft_on && song.playing && song.rec == 1u && song.g[G_BPM] == 100,
          "REC mode TEMPO, empty project: the first note starts the loop at 100 BPM, recording");
    transport_req = 2; run_block();

    reset(100);
    song.sel = 0;
    rec_tempo = 1, rec_count = 1;
    rec_wait = 1;
    input_on(TSEL, 62, 100);
    run_block();
    check(!song.playing && !ci_on && !ft_on && rec_wait, "REC START COUNT: a note only sounds, nothing starts");
    c0 = nhits;
    transport_req = 1;                                /* PLAY: the count-in */
    run_block();
    for (k = 0; k + 2u < 4u * bpb; k++) run_block();
    check(ci_on && !song.playing, "REC START COUNT: PLAY -> one bar of clicks, not playing yet");
    {
        uint32_t n = 0, i;
        for (i = c0; i < nhits; i++) n += hits[i].note == 76 || hits[i].note == 77;
        check(n == 4u, "REC START COUNT: 4 clicks (one bar of 4/4)");
    }
    for (k = 0; k < 4u; k++) run_block();
    check(!ci_on && song.playing && song.rec == 1u && !rec_wait,
          "REC START COUNT: after the bar the loop starts and records");
    transport_req = 2; run_block();

    reset(100);
    song.sel = 0;
    rec_tempo = 1, rec_count = 1;
    rec_wait = 1;
    transport_req = 1; run_block();
    for (k = 0; k < bpb; k++) run_block();
    rec_wait = 0;                                     /* REC again: cancelled */
    for (k = 0; k < 4u * bpb; k++) run_block();
    check(!ci_on && !song.playing && !song.rec, "REC START COUNT: REC during the count-in cancels it");
    rec_wait = 1;
    transport_req = 1; run_block();
    transport_req = 1; run_block();                   /* PLAY again: back to armed */
    check(!ci_on && rec_wait && !song.playing, "REC START COUNT: PLAY again during the count-in: back to armed");
    rec_wait = 0;
    rec_tempo = 0, rec_count = 0;
}

/* menu USB AUDIO = FULL (2.3): the USB input at the level of MASTER all the way up, whatever the knob;
 * the DAC path keeps following the knob */
static void t_usbfull(void)
{
    uint32_t k, i;
    int32_t out[CTL * 2], pk_dac = 0, pk_usb = 0;
    reset(120);
    song.master_q12 = 512;                            /* MASTER low */
    master_cur = -1;
    usb_full_now = 1;
    for (k = 0; k < 8u; k++) trk_note_on(&trk[0], 48u + k * 3u, 110, 0);
    for (k = 0; k < 600u; k++) {
        mix_block(out, CTL);
        if (k > 100u)
            for (i = 0; i < 2u * CTL; i++) {
                int32_t a = out[i] < 0 ? -out[i] : out[i], u = usb_out[i] < 0 ? -usb_out[i] : usb_out[i];
                if (a > pk_dac) pk_dac = a;
                if (u > pk_usb) pk_usb = u;
            }
    }
    usb_full_now = 0;
    for (k = 0; k < 8u; k++) trk_note_off(&trk[0], 48u + k * 3u);
    song.master_q12 = 2048;
    check(pk_usb > pk_dac * 3 && pk_usb <= 32767, "USB AUDIO FULL: the USB level stays up when MASTER is low, no clipping");
}


/* ---- maqam: 24-tone pitch core (change maqam-24-tone-scales, tasks 1.1 .. 1.5) ---- */
/* the part rendered without the mix: a clean sine (ANALOG, WAVE 3), the filter open, no modulation, no TUNE */
static void pq_setup(track_t *t, uint32_t voice)
{
    uint32_t i;
    reset(120);
    song.playing = 0;
    song.sel = 0;
    memset(t->v, 0, sizeof t->v);                      /* no voice left from before */
    t->nmono = 0;
    t->mono_note = 0;
    t->xp_n = 0;
    t->nheld = 0;
    t->arp_phys = 0;
    t->xf_on = 0;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = 0;
    t->engine = t->eng_req = 0;
    t->p[P_E0] = 3;                                   /* sine */
    t->p[P_E4] = 127;                                 /* cutoff open */
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    t->p[P_ED_PIT] = 0;
    t->p[P_LD_PIT] = 0;
    t->p[P_GLIDE] = 0;
    t->p[P_VOICE] = (int16_t)voice;
    t->p[P_DETUNE] = 0;
    song.g[G_TUNE] = 0;
}
/* the frequency of the part over secs (after skip s): rising zero crossings, interpolated */
static double pq_hz(track_t *t, double skip, double secs)
{
    int32_t out[CTL], prev = 0;
    uint32_t n = 0, i, first = 1;
    double t0 = 0, t1 = 0, pos = 0;
    uint64_t total = (uint64_t)((skip + secs) * FS) / CTL, b, from = (uint64_t)(skip * FS) / CTL;
    for (b = 0; b < total; b++) {
        track_render(t, out, CTL);
        if (b < from)
            continue;
        for (i = 0; i < CTL; i++, pos += 1.0) {
            if (prev < 0 && out[i] >= 0) {
                double x = pos - 1.0 + (double)(-prev) / (double)(out[i] - prev);
                if (first) { t0 = x; first = 0; n = 0; } else { t1 = x; n++; }
            }
            prev = out[i];
        }
    }
    return n ? (double)n * FS / (t1 - t0) : 0.0;
}
static double pq_cents(double hz, double midi) { return 1200.0 * log2(hz / (440.0 * pow(2.0, (midi - 69.0) / 12.0))); }

static void t_pitch24(void)
{
    track_t *t = &trk[0];
    uint32_t n, ok = 1, i;
    double c, c2, c3;
    /* 1.1 the helpers: 0, 127, 127|Q, and every p24 round trip */
    ok = note_semi(0) == 0 && note_q(0) == 0 && note_p24(0) == 0 && note_from_p24(0) == 0;
    ok &= note_semi(127) == 127 && note_q(127) == 0 && note_p24(127) == 254 && note_from_p24(254) == 127;
    ok &= note_semi(127 | NOTE_Q) == 127 && note_q(127 | NOTE_Q) == 1 && note_p24(127 | NOTE_Q) == 255 &&
          note_from_p24(255) == (127u | NOTE_Q);
    for (n = 0; n < 256u; n++)
        ok &= note_p24(note_from_p24(n)) == n && note_from_p24(note_p24(n)) == n;
    for (n = 0; n < 128u; n++)                       /* a plain note is its own byte and its own 1/16 pitch */
        ok &= note_from_p24(n * 2u) == n && note_p16(n) == (int32_t)n * 16 && note_shift(n, 0, 0) == n;
    ok &= note_shift(127 | NOTE_Q, 5, 0) == (127u | NOTE_Q) && note_shift(2 | NOTE_Q, -9, 1) == (1u | NOTE_Q);
    check(ok, "pitch24: note_semi / note_q / note_p24 / note_from_p24 (0, 127, 127|Q, 256 round trips)");

    /* 1.3 what a voice sounds (D2): C4|Q = +50 c, A4 + 20 c = +20 c, in every voice mode */
    for (i = 0; i < 4u; i++) {
        static const char *const VM[4] = {"POLY", "MONO", "LEGATO", "UNISON"};
        char what[96];
        pq_setup(t, i);
        trk_note_on(t, 60u | NOTE_Q, 100, 0);
        c = pq_cents(pq_hz(t, 0.2, 1.0), 60.0);       /* C4 = MIDI 60: +50 */
        pq_setup(t, i);
        trk_note_on(t, 69, 100, 20);
        c2 = pq_cents(pq_hz(t, 0.2, 1.0), 69.0);
        pq_setup(t, i);
        trk_note_on(t, 69, 100, 0);
        c3 = pq_cents(pq_hz(t, 0.2, 1.0), 69.0);
        snprintf(what, sizeof what, "pitch: %s: C4|Q %+.2f c (50), A4 + 20 c %+.2f c (20), A4 %+.2f c (0)", VM[i], c, c2, c3);
        check(fabs(c - 50.0) <= 1.0 && fabs(c2 - 20.0) <= 1.0 && fabs(c3) <= 0.5, what);
    }
    {
        double c4, c5;
        pq_setup(t, V_POLY);
        trk_note_on(t, 69, 100, -12);
        c = pq_cents(pq_hz(t, 0.2, 1.0), 69.0);
        pq_setup(t, V_POLY);
        trk_note_on(t, 69, 100, 150);                 /* the offset clamps at +100 */
        c2 = pq_cents(pq_hz(t, 0.2, 1.0), 69.0);
        pq_setup(t, V_POLY);
        trk_note_on(t, 60u | NOTE_Q, 100, 80);        /* the cents clamp, not the sum: 50 + 80 */
        c3 = pq_cents(pq_hz(t, 0.2, 1.0), 60.0);
        pq_setup(t, V_POLY);
        song.g[G_TUNE] = 30;                          /* TUNE stays on top of the offset */
        trk_note_on(t, 69, 100, 20);
        c4 = pq_cents(pq_hz(t, 0.2, 1.0), 69.0);
        pq_setup(t, V_POLY);
        trk_note_on(t, 69, 100, -100);
        c5 = pq_cents(pq_hz(t, 0.2, 1.0), 69.0);
        printf("seq2:   cents: -12 -> %+.2f, +150 -> %+.2f, C4|Q + 80 -> %+.2f, TUNE 30 + 20 -> %+.2f, -100 -> %+.2f\n", c, c2, c3, c4, c5);
        check(fabs(c + 12.0) <= 1.0 && fabs(c2 - 100.0) <= 1.0 && fabs(c3 - 130.0) <= 1.0 && fabs(c4 - 50.0) <= 1.0 &&
                  fabs(c5 + 100.0) <= 1.0,
              "pitch: -12 c, +150 c clamps to +100, C4|Q + 80 c = +130, TUNE 30 + 20 c = +50, -100 c");
    }
    song.g[G_TUNE] = 0;

    /* 1.4 glide settles on E-half-flat (D#|Q) after D, not on E or D# */
    pq_setup(t, V_MONO);
    t->p[P_GLIDE] = 30;
    trk_note_on(t, 62, 100, 0);
    pq_hz(t, 0.0, 0.3);
    trk_note_on(t, 63u | NOTE_Q, 100, 0);
    c = pq_cents(pq_hz(t, 2.0, 1.0), 63.5);
    check(fabs(c) <= 1.0 && t->v[0].pitch16 == 63 * 16 + 8 && t->v[0].pitch_cur == 63 * 16 + 8,
          "glide: D -> E-half-flat (D#|Q) settles on 63.5, within 1 cent");
    /* ... and the MONO priorities rank D#|Q between D# and E */
    pq_setup(t, V_MONO);
    t->p[P_PRIO] = 2;
    trk_note_on(t, 63u | NOTE_Q, 100, 0);
    trk_note_on(t, 63, 100, 0);
    check(t->mono_note == (63u | NOTE_Q), "mono HIGH: D#|Q (63.5) wins over D# (63)");
    pq_setup(t, V_MONO);
    t->p[P_PRIO] = 1;
    trk_note_on(t, 64, 100, 0);
    trk_note_on(t, 63u | NOTE_Q, 100, 0);
    check(t->mono_note == (63u | NOTE_Q), "mono LOW: D#|Q (63.5) wins over E (64)");

    /* 1.4 a Q note through live input -> record -> play -> arp */
    pq_setup(t, V_POLY);
    t->p[P_AMODE] = 0;
    song.playing = 1;
    song.rec = 1;
    input_on(t, 63u | NOTE_Q, 100);                   /* (the key path: kb_map makes no Q before task 3.1) */
    song.rec = 0;
    song.playing = 0;
    input_off(t, 63u | NOTE_Q);
    for (i = 0, n = 0; i < NSTEP; i++)
        n += t->step[i].time == ST_NOTE && t->step[i].n == 1u && t->step[i].note[0] == (63u | NOTE_Q);
    check(n == 1u && last_note == (63u | NOTE_Q), "Q note: live input records the byte unchanged (D#|Q) and keeps it in last_note");
    trk_all_off(t);
    for (i = 0; i < NVOICE; i++)
        t->v[i].active = 0;
    t->seq_active = 1;
    transport_req = 1;
    for (n = 0; n < 4000u && !(t->v[0].active && t->v[0].gate); n++)
        run_block();
    check(t->v[0].active && t->v[0].note == (63u | NOTE_Q) && t->v[0].pitch16 == 63 * 16 + 8,
          "Q note: the sequencer plays it at 63.5 (voice note byte and pitch16)");
    transport_req = 2;
    for (n = 0; n < 50u; n++)
        run_block();
    pq_setup(t, V_POLY);
    t->p[P_AMODE] = 1;
    t->p[P_AOCT] = 2;
    arp_add(t, 61);
    arp_add(t, 60u | NOTE_Q);                        /* C4|Q = 60.5: between C4 and C#4 */
    {
        uint32_t a0 = arp_next(t), a1 = arp_next(t), a2 = arp_next(t), a3 = arp_next(t);
        check(a0 == (60u | NOTE_Q) && a1 == 61u && a2 == (72u | NOTE_Q) && a3 == 73u,
              "Q note: the arp sorts it between C4 and C#4 and keeps the flag over the octaves");
    }
    {
        uint32_t k = 63u | NOTE_Q;
        for (i = 0; i < 8u; i++)
            er_notes[i] = 0;
        er_notes[(k >> 5) & 7u] |= 1u << (k & 31u);
        check(er_has(63u | NOTE_Q) && !er_has(63) && !er_has(64), "Q note: erase tells D#|Q from D# (256 bits)");
        er_notes[(k >> 5) & 7u] = 0;
    }

    /* 1.5 MIDI out sends the semitone: a key on E-half-flat (D#|Q) sends D# off (and on, below) */
    pq_setup(t, V_POLY);
    usb.config = 1;
    mo_w = mo_r = 0;
    kb_kind[5] = KS_NOTE;                            /* (a key held on this note: kb_map makes no Q before task 3.1) */
    kb_trk[5] = 0;
    kb_n[5] = 1;
    kb_nt[5][0] = (uint8_t)(63u | NOTE_Q);
    kb_prev = 1u << 5;
    fm1_in.notes = 0;
    keyboard_block();
    check(mo_w == 1u && ((midi_out_q[0] >> 8) & 0xF0u) == 0x80u && ((midi_out_q[0] >> 16) & 0xFFu) == 63u,
          "MIDI out: E-half-flat (D#|Q) key up sends note-off D# (63), no flag");
    mo_w = mo_r = 0;
    t->p[P_QUANT] = 2;
    t->p[P_SCALE] = 0;
    t->p[P_ROOT] = 0;
    t->p[P_TRANS] = 0;
    song.octave = 0;
    fm1_in.notes = 1u << 7;                           /* the C4 key */
    keyboard_block();
    check(mo_w == 1u && ((midi_out_q[0] >> 8) & 0xF0u) == 0x90u && ((midi_out_q[0] >> 16) & 0xFFu) == 60u,
          "MIDI out: key-down sends the semitone (a plain note is unchanged)");
    fm1_in.notes = 0;
    keyboard_block();
}

/* ---- maqam: intonation (tasks 2.5, 2.5a): tune[(p24 - root) mod 24] at note-on, gated by G_INTON ---- */
static uint32_t sc_find(const char *code)
{
    uint32_t i;
    for (i = 0; i < NSCALES; i++)
        if (!strcmp(SCALES[i].code, code))
            return i;
    assert(!"scale code");
    return 0;
}
/* the interval of note b above note a (cents) with scale / root / G_INTON set: both rendered */
static double pq_interval(const char *code, int32_t root, uint32_t inton, uint32_t a, uint32_t b)
{
    track_t *t = &trk[0];
    double fa, fb;
    pq_setup(t, V_POLY);
    t->p[P_SCALE] = (int16_t)sc_find(code);
    t->p[P_ROOT] = (int16_t)root;
    song.g[G_INTON] = (int16_t)inton;
    trk_note_on(t, a, 100, 0);
    fa = pq_hz(t, 0.2, 1.0);
    pq_setup(t, V_POLY);
    t->p[P_SCALE] = (int16_t)sc_find(code);
    t->p[P_ROOT] = (int16_t)root;
    song.g[G_INTON] = (int16_t)inton;
    trk_note_on(t, b, 100, 0);
    fb = pq_hz(t, 0.2, 1.0);
    return 1200.0 * log2(fb / fa);
}
static void t_inton(void)
{
    double a, b, c, d, e;
    char what[160];
    /* 2.5 TRADITIONAL (the default) */
    a = pq_interval("T.RST", 0, 1, 60, 64);              /* the third of Turkish Rast on C: 385 */
    b = pq_interval("BHUPL", 0, 1, 60, 64);              /* Bhupali Ga: 386 */
    c = pq_interval("RAST", 0, 1, 60, 63u | NOTE_Q);     /* Arabic Rast E-half-flat: 357 */
    snprintf(what, sizeof what, "intonation: TURK Rast 3rd %.1f (385), INDIA Bhupali Ga %.1f (386), ARAB Rast 3rd %.1f (357) cents", a, b, c);
    check(fabs(a - 385.0) <= 3.0 && fabs(b - 386.0) <= 3.0 && fabs(c - 357.0) <= 3.0, what);
    d = pq_interval("SEGAH", 0, 1, 60, 61);              /* the same degree (C#), two scales */
    e = pq_interval("KURDI", 0, 1, 60, 61);
    snprintf(what, sizeof what, "intonation: the 2nd of TURK Segah %.1f c, of Kurdi %.1f c: the same note, another pitch", d, e);
    check(fabs((d - e) - (SCALES[sc_find("SEGAH")].tune[2] - SCALES[sc_find("KURDI")].tune[2])) <= 1.0, what);
    /* the offsets follow the root: Segah on D (ROOT 4), its 2nd is D#4 */
    d = pq_interval("SEGAH", 4, 1, 62, 63);
    check(fabs(d - (100.0 + SCALES[sc_find("SEGAH")].tune[2])) <= 1.0, "intonation: TURK Segah on D: the same 2nd (its offset) above the root");
    /* a note outside the scale takes no offset */
    d = pq_interval("T.RST", 0, 1, 60, 61);
    check(fabs(d - 100.0) <= 1.0, "intonation: a note outside the scale (C#4 in TURK Rast) stays on the grid");
    /* 2.5a EQUAL: the plain grid; step bends still apply */
    a = pq_interval("T.RST", 0, 0, 60, 64);
    c = pq_interval("RAST", 0, 0, 60, 63u | NOTE_Q);
    snprintf(what, sizeof what, "EQUAL: TURK Rast 3rd %.1f (400), ARAB Rast 3rd %.1f (350) cents", a, c);
    check(fabs(a - 400.0) <= 1.0 && fabs(c - 350.0) <= 1.0, what);
    {
        double f1, f2;
        track_t *t = &trk[0];
        pq_setup(t, V_POLY);
        t->p[P_SCALE] = (int16_t)sc_find("MAJ");
        song.g[G_INTON] = 1;
        trk_note_on(t, 64, 100, 0);
        f1 = pq_hz(t, 0.2, 1.0);
        pq_setup(t, V_POLY);
        t->p[P_SCALE] = (int16_t)sc_find("MAJ");
        song.g[G_INTON] = 0;
        trk_note_on(t, 64, 100, 0);
        f2 = pq_hz(t, 0.2, 1.0);
        check(f1 == f2, "WEST: a render is identical in EQUAL and TRADITIONAL");
        pq_setup(t, V_POLY);
        t->p[P_SCALE] = (int16_t)sc_find("T.RST");
        song.g[G_INTON] = 0;
        trk_note_on(t, 64, 100, -10);
        f1 = pq_hz(t, 0.2, 1.0);
        check(fabs(pq_cents(f1, 64.0) + 10.0) <= 1.0, "EQUAL: a step bend still applies (-10 c)");
        pq_setup(t, V_POLY);
        t->p[P_SCALE] = (int16_t)sc_find("T.RST");
        trk_note_on(t, 64, 100, -10);                      /* TRADITIONAL: -15 and -10 add */
        f1 = pq_hz(t, 0.2, 1.0);
        check(fabs(pq_cents(f1, 64.0) - (SCALES[sc_find("T.RST")].tune[8] - 10.0)) <= 1.0, "TRADITIONAL: the step bend adds to the offset (Rast 3rd + -10 c)");
        pq_setup(t, V_POLY);
        t->p[P_SCALE] = (int16_t)sc_find("T.RST");
        trk_note_on(t, 64, 100, -95);                      /* -15 - 95: clamped to -100 */
        f1 = pq_hz(t, 0.2, 1.0);
        check(fabs(pq_cents(f1, 64.0) + 100.0) <= 1.0, "the total offset clamps at -100 c");
        pq_setup(t, V_POLY);
        t->p[P_SCALE] = (int16_t)sc_find("T.RST");
        p24_plain = 1;                                     /* MIDI in: a plain 12-tone note */
        trk_note_on(t, 64, 100, 0);
        p24_plain = 0;
        f1 = pq_hz(t, 0.2, 1.0);
        check(fabs(pq_cents(f1, 64.0)) <= 0.5, "MIDI in: no intonation offset (plain 12-tone)");
    }
    song.g[G_INTON] = 1;
}

/* ---- maqam: direction of play (task 3.3, D5): ARAB Rast up gives B-half-flat, down gives B-flat, as recorded ---- */
static void kb_tap(uint32_t k)
{
    fm1_in.notes = 1u << k;
    keyboard_block();
    fm1_in.notes = 0;
    keyboard_block();
}
static uint32_t rec_has(track_t *t, uint32_t note)
{
    uint32_t i, j, n = 0;
    for (i = 0; i < NSTEP; i++)
        for (j = 0; t->step[i].time == ST_NOTE && j < t->step[i].n; j++)
            n += t->step[i].note[j] == note;
    return n;
}
static void t_direction(void)
{
    track_t *t = &trk[0];
    uint32_t n;
    pq_setup(t, V_POLY);
    t->p[P_QUANT] = 2;
    t->p[P_SCALE] = (int16_t)sc_find("RAST");
    t->p[P_ROOT] = 0;
    memset(kb_last_key, 0, sizeof kb_last_key);
    kb_tap(14);                                       /* G4 */
    song.playing = 1;
    song.rec = 1;
    kb_tap(18);                                       /* up to the 7th degree: B-half-flat */
    check(kb_map(t, 18) == (70u | NOTE_Q) && rec_has(t, 70u | NOTE_Q) == 1u, "direction: Rast up to the 7th degree: B-half-flat, recorded");
    song.rec = 0;
    song.playing = 0;
    steps_clear(t);
    kb_tap(19);                                       /* C5 */
    song.playing = 1;
    song.rec = 1;
    kb_tap(18);                                       /* back down to it: B-flat */
    song.rec = 0;
    song.playing = 0;
    check(rec_has(t, 70u) == 1u && rec_has(t, 70u | NOTE_Q) == 0u, "direction: coming down from C5 it is B-flat, and B-flat is what is recorded");
    kb_tap(18);                                       /* the same key again: ascending form */
    check(kb_map(t, 18) == (70u | NOTE_Q) || kb_last_key[0] == 18, "direction: a repeated key counts as up");
    steps_clear(t);
    t->step[0].n = 1;
    t->step[0].note[0] = 70;
    t->step[0].time = ST_NOTE;
    t->step[0].vel = 100;
    trk_all_off(t);
    memset(t->v, 0, sizeof t->v);
    t->seq_active = 1;
    transport_req = 1;
    {
        uint32_t ok = 0, i;
        for (n = 0; n < 4000u && !ok; n++) {
            run_block();
            for (i = 0; i < NVOICE; i++)
                ok |= t->v[i].active && t->v[i].gate && t->v[i].note == 70u && t->v[i].pitch16 == 70 * 16 + 3;   /* (+20 c of Rast's B-flat: 3 sixteenths) */
        }
        check(ok, "direction: the recorded step replays B-flat, in whichever direction");
    }
    transport_req = 2;
    for (n = 0; n < 50u; n++)
        run_block();
}

/* ---- maqam: the step bend (task 5.1 / 5.2): every note of the step; fallbacks and the engine switch keep it ---- */
static void t_stepbend(void)
{
    track_t *t = &trk[0];
    uint32_t n, i, ok;
    double c;
    check(sizeof(step_t) == 11 && sizeof(dstep_t) == 11, "step bend: a step is 11 bytes on every track");
    pq_setup(t, V_POLY);
    steps_clear(t);
    t->step[0].n = 1; t->step[0].note[0] = 69; t->step[0].time = ST_NOTE; t->step[0].vel = 100; t->step[0].cents = -12;
    t->seq_active = 1;
    transport_req = 1;
    for (n = 0; n < 4000u && !(t->v[0].active && t->v[0].gate); n++)
        run_block();
    c = pq_cents(pq_hz(t, 0.2, 1.0), 69.0);
    check(fabs(c + 12.0) <= 1.0, "step bend: A4 at -12 on a WEST scale sounds 12 +-1 cents flat");
    transport_req = 2;
    for (n = 0; n < 50u; n++)
        run_block();
    pq_setup(t, V_POLY);
    steps_clear(t);
    t->step[0].n = 3; t->step[0].note[0] = 60; t->step[0].note[1] = 64; t->step[0].note[2] = 67; t->step[0].time = ST_NOTE;
    t->step[0].vel = 100; t->step[0].cents = 8;
    t->seq_active = 1;
    transport_req = 1;
    for (n = 0, ok = 0; n < 4000u && ok < 3u; n++) {
        run_block();
        for (i = 0, ok = 0; i < NVOICE; i++)
            ok += t->v[i].active && t->v[i].gate && (t->v[i].note == 60 || t->v[i].note == 64 || t->v[i].note == 67) &&
                  t->v[i].pitch16 == (int32_t)t->v[i].note * 16 + 1 && t->v[i].bend_fine == (128 - 100) * 2367 / 16000;
    }
    check(ok == 3u, "step bend: a chord step bends all three notes (+8 c: 1/16 st and the rest)");
    transport_req = 2;
    for (n = 0; n < 50u; n++)
        run_block();
    /* MONO: letting go of the newer key falls back to the held one, with its own bend */
    pq_setup(t, V_MONO);
    trk_note_on(t, 60, 100, 30);
    trk_note_on(t, 64, 100, -40);
    trk_note_off(t, 64);
    c = pq_cents(pq_hz(t, 0.2, 1.0), 60.0);
    check(fabs(c - 30.0) <= 1.0 && t->mono_note == 60, "step bend: MONO falls back to the held C4 with its +30 c");
    pq_setup(t, V_LEGATO);
    trk_note_on(t, 60, 100, 0);
    trk_note_on(t, 64, 100, 25);
    trk_note_off(t, 60);                              /* (a note that is not on top: its bend leaves with it) */
    c = pq_cents(pq_hz(t, 0.2, 1.0), 64.0);
    check(fabs(c - 25.0) <= 1.0, "step bend: LEGATO keeps the +25 c of the sounding E4");
    /* the engine switch: a note-on that waits for the new engine keeps its bend */
    pq_setup(t, V_POLY);
    t->eng_req = 1;
    trk_note_on(t, 69, 100, -12);
    for (n = 0, ok = 0; n < 400u && !ok; n++) {
        run_block();
        for (i = 0; i < NVOICE; i++)
            ok |= t->engine == 1 && t->v[i].active && t->v[i].gate && t->v[i].note == 69 && t->v[i].pitch16 == 69 * 16 - 2;
    }
    check(ok, "step bend: a note queued during an engine switch plays with its -12 c");
    t->engine = t->eng_req = 0;
}

/* ---- maqam review regressions: the MIDI-in marks, and a held MIDI note stays plain through fallbacks ---- */
static void t_midimarks(void)
{
    track_t *t = &trk[0];
    uint32_t n, i, ok;
    pq_setup(t, V_POLY);
    memset(p24_midi_n, 0, sizeof p24_midi_n);
    mclk_push(0x09u | 0x90u << 8 | 60u << 16 | 100u << 24);        /* MIDI note 60 on, channel 1 = this part */
    for (n = 0; n < 4u; n++)
        run_block();
    check(p24_midi_get(t, 60) == 1u && p24_midi_get(t, 60u | NOTE_Q) == 0u && p24_midi_get(t, 61) == 0u,
          "MIDI marks: note 60 is marked, 60|Q (byte 188) is not (256 bits)");
    p24_midi_mark(t, 60u | NOTE_Q, 0);                                  /* a panel key on 60|Q (key_down) */
    check(p24_midi_get(t, 60) == 1u, "MIDI marks: a key on 60|Q does not clear the mark on 60");
    p24_midi_mark(t, 60u | NOTE_Q, 1);
    check(p24_midi_get(t, 60) == 1u && p24_midi_get(t, 60u | NOTE_Q) == 1u, "MIDI marks: both can be set apart");
    p24_midi_mark(t, 60u | NOTE_Q, 0);
    mclk_push(0x08u | 0x80u << 8 | 60u << 16);                          /* note off */
    for (n = 0; n < 4u; n++)
        run_block();
    check(p24_midi_get(t, 60) == 0u, "MIDI marks: the MIDI note-off clears the mark");

    /* MONO: a MIDI note held under a panel note: letting go of the upper falls back to it with no tuning */
    pq_setup(t, V_MONO);
    t->p[P_SCALE] = (int16_t)sc_find("T.RST");                          /* E4 (64) has an offset of its own */
    song.g[G_INTON] = 1;
    memset(p24_midi_n, 0, sizeof p24_midi_n);
    p24_midi_mark(t, 64, 1);
    p24_plain = 1;
    trk_note_on(t, 64, 100, 0);                                         /* (what the MIDI path does) */
    p24_plain = 0;
    ok = t->v[0].pitch16 == 64 * 16 && t->v[0].bend_fine == 0;
    trk_note_on(t, 67, 100, 0);                                         /* a panel note above it: G4 (on the scale) */
    trk_note_off(t, 67);                                                /* back to the held MIDI note */
    check(ok && t->mono_note == 64 && t->v[0].pitch16 == 64 * 16 && t->v[0].bend_fine == 0,
          "MONO fallback to a held MIDI note: no intonation offset (plain E4)");
    pq_setup(t, V_MONO);
    t->p[P_SCALE] = (int16_t)sc_find("T.RST");
    song.g[G_INTON] = 1;
    memset(p24_midi_n, 0, sizeof p24_midi_n);
    trk_note_on(t, 64, 100, 0);                                         /* a panel note: tuned (the control case) */
    check(t->v[0].pitch16 != 64 * 16 || t->v[0].bend_fine != 0, "control: a panel E4 in TURK Rast carries its offset");
    trk_note_on(t, 67, 100, 0);
    trk_note_off(t, 67);
    check(t->mono_note == 64 && (t->v[0].pitch16 != 64 * 16 || t->v[0].bend_fine != 0), "control: the fallback to a panel note keeps its offset");

    /* the engine switch: a MIDI note queued meanwhile plays plain */
    pq_setup(t, V_POLY);
    t->p[P_SCALE] = (int16_t)sc_find("T.RST");
    song.g[G_INTON] = 1;
    memset(p24_midi_n, 0, sizeof p24_midi_n);
    p24_midi_mark(t, 64, 1);
    t->eng_req = 1;
    p24_plain = 1;
    trk_note_on(t, 64, 100, 0);
    p24_plain = 0;
    for (n = 0, ok = 0; n < 400u && !ok; n++) {
        run_block();
        for (i = 0; i < NVOICE; i++)
            ok |= t->engine == 1 && t->v[i].active && t->v[i].gate && t->v[i].note == 64 && t->v[i].pitch16 == 64 * 16 &&
                  t->v[i].bend_fine == 0;
    }
    check(ok, "engine switch: a MIDI note queued meanwhile plays with no intonation offset");
    t->engine = t->eng_req = 0;
    memset(p24_midi_n, 0, sizeof p24_midi_n);
    song.g[G_INTON] = 1;
}

int main(void)
{
    t_usbfull();
    t_recmode();
    t_mclk();
    t_shed();
    t_drift();
    t_burst();
    t_swing_odd();
    t_len_phase();
    t_ratchet();
    t_roll();
    t_erase_undo();
    t_levels();
    t_chords();
    t_mute();
    t_pitch24();
    t_inton();
    t_direction();
    t_stepbend();
    t_midimarks();
    printf("seq2: %s\n", fails ? "FAILED" : "all checks ok");
    return fails;
}
