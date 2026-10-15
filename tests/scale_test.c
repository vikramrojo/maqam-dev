/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the real keyboard, recording, arp and MIDI-out paths on the host.
 * Build with the same generated headers and flags as hostsim.c. */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static const uint8_t WHITE_KEYS[] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const struct { uint8_t count, notes[12]; } EXPECTED[] = {
    {12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {7, {0, 2, 4, 5, 7, 9, 11}}, {7, {0, 2, 3, 5, 7, 8, 10}},
    {7, {0, 2, 3, 5, 7, 9, 10}}, {7, {0, 2, 4, 5, 7, 9, 10}},
    {5, {0, 2, 4, 7, 9}}, {5, {0, 3, 5, 7, 10}},
    {7, {0, 2, 3, 5, 7, 8, 11}}, {7, {0, 1, 3, 5, 7, 8, 10}},
    {7, {0, 2, 4, 6, 7, 9, 11}}, {7, {0, 1, 3, 5, 6, 8, 10}},
    {7, {0, 2, 3, 5, 7, 9, 11}}, {6, {0, 3, 5, 6, 7, 10}},
    {6, {0, 2, 4, 6, 8, 10}}, {8, {0, 1, 3, 4, 6, 7, 9, 10}},
    {8, {0, 2, 3, 5, 6, 8, 9, 11}},
};

static void mapping_test(void)
{
    track_t *t = &trk[0];
    uint32_t s, k, w;
    int root, oct, trans;
    assert(TP[P_SCALE].max + 1 == NSCALES && NSCALES == 52);   /* maqam: 52 scales, the WEST rows are 0..15 */
    t->p[P_QUANT] = 2;
    for (s = 0; s < sizeof EXPECTED / sizeof EXPECTED[0]; s++) {   /* maqam: the WEST scales */
        uint32_t mask = 0;
        t->p[P_SCALE] = (int16_t)s;
        for (k = 0; k < EXPECTED[s].count; k++)
            mask |= 1u << EXPECTED[s].notes[k];
        assert(mask12_of(scale_mask(t)) == mask);       /* maqam: the 24-bit mask holds them on the even slots */
        assert(scale_mask(t) == SCALES[s].desc && SCALES[s].set == SET_WEST);
        for (root = 0; root < 12; root++)
            for (oct = -3; oct <= 3; oct++)
                for (trans = -24; trans <= 24; trans++) {
                    t->p[P_ROOT] = (int16_t)(2 * root);   /* maqam: quarter tones */
                    song.octave = (int8_t)oct;
                    t->p[P_TRANS] = (int16_t)trans;
                    for (k = w = 0; k < 27u; k++) {
                        uint32_t actual = kb_map(t, k);
                        if (k == WHITE_KEYS[w]) {
                            /* Four white keys precede C4; use a positive cycle
                             * offset to independently handle the lower degrees. */
                            int d = (int)w - 4 + 12 * EXPECTED[s].count;
                            int want = 60 + root + 12 * (oct + d / EXPECTED[s].count - 12)
                                + EXPECTED[s].notes[d % EXPECTED[s].count] + trans;
                            assert(actual == (uint32_t)clamp(want, 0, 127));
                            w++;
                        } else {
                            assert(actual == KB_SILENT);
                        }
                    }
                }
    }
    t->p[P_QUANT] = 0;
    song.octave = 0;
    t->p[P_TRANS] = -5;
    for (k = 0; k < 27u; k++)
        assert(kb_map(t, k) == 48u + k);
    t->p[P_QUANT] = 2;
    for (k = 0; k < 27u; k++) {                /* the drum track: the key's lane, whatever the scale */
        TDRUM->p[P_QUANT] = 2;
        assert(kb_map(TDRUM, k) == LANE_NOTE[lane_of_key(k)]);
    }
    t->engine = t->eng_req = 4;
    if (drum_set() >= 0) {
        t->p[P_E0] = (int16_t)drum_set();
        for (k = 0; k < 27u; k++)
            assert(kb_map(t, k) == 36u + k);
    }
    t->engine = t->eng_req = 0;                /* SNAP (QNT 1, the old ON): every key, rounded down */
    t->p[P_QUANT] = 1;
    t->p[P_SCALE] = 2;                         /* C minor */
    t->p[P_ROOT] = 0;
    t->p[P_TRANS] = 0;
    song.octave = 0;
    assert(kb_map(t, 11) == 63u && kb_map(t, 10) == 63u && kb_map(t, 7) == 60u && kb_map(t, 8) == 60u);
    puts("scales: all 16 scales, 12 roots, octave/transpose ranges, bypass, drums and SNAP ok");
}

static void key_events_test(void)
{
    track_t *t = &trk[0];
    uint32_t before;
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    kb_prev = 0;
    usb.config = 1;
    mo_w = mo_r = 0;
    t->p[P_QUANT] = 2;
    t->p[P_SCALE] = 2;                      /* C minor: E key plays Eb */
    t->p[P_AMODE] = 1;
    song.playing = song.rec = 1;
    fm1_in.notes = 1u << 8;                /* C# is silent */
    keyboard_block();
    assert(t->arp_phys == 0 && t->nheld == 0 && t->step[0].n == 0 && mo_w == 0);
    t->p[P_QUANT] = 0;                    /* releasing a muted key stays silent */
    fm1_in.notes = 0;
    keyboard_block();
    assert(mo_w == 0);
    t->p[P_QUANT] = 2;
    fm1_in.notes = (1u << 11) | (1u << 10); /* E and D#: only Eb sounds/records */
    keyboard_block();
    assert(t->arp_phys == 1 && t->nheld == 1 && t->held[0] == 63);
    assert(t->step[0].n == 0);             /* ARP on: what it plays is recorded, not the key */
    arp_tick(t, CTL * (uint32_t)song.g[G_BPM]);
    assert(t->step[0].n == 1 && t->step[0].note[0] == 63);
    assert(mo_w == 1 && ((midi_out_q[0] >> 16) & 127u) == 63);
    t->p[P_SCALE] = 9;
    t->p[P_ROOT] = 12;                     /* maqam: F# (quarter tones) */
    t->p[P_TRANS] = 12;
    song.octave = 1;
    song.sel = 1;                          /* key-up follows the original note/part */
    fm1_in.notes = 0;
    keyboard_block();
    assert(t->arp_phys == 0 && t->nheld == 0);
    assert(mo_w == 2 && ((midi_out_q[1] >> 16) & 127u) == 63);
    assert(((midi_out_q[1] >> 8) & 255u) == 0x80u);
    song.sel = 0;
    t->p[P_QUANT] = 0;
    fm1_in.notes = 1u << 8;                /* held black key must release after enabling mode */
    keyboard_block();
    before = mo_w;
    assert(t->arp_phys == 1);
    t->p[P_QUANT] = 2;
    fm1_in.notes = 0;
    keyboard_block();
    assert(t->arp_phys == 0 && t->nheld == 0 && mo_w == before + 1);
    puts("scales: silent keys, arp, live recording, MIDI out and held-note changes ok");
}


/* maqam (tasks 2.3 / 2.6): the 52 scales, frozen: index order (append-only), set, and the 24-bit ascending /
 * descending masks (bit = quarter-tone slot from the root). Copied from design.md's appendix; the WEST rows are
 * EXPECTED above, spread onto the even slots. */
static const struct { const char *code; uint8_t set; uint32_t asc, desc; } FROZEN[52] = {
    {"CHR", SET_WEST, 0x555555u, 0x555555u}, {"MAJ", SET_WEST, 0x444511u, 0x444511u}, {"MIN", SET_WEST, 0x114451u, 0x114451u},
    {"DOR", SET_WEST, 0x144451u, 0x144451u}, {"MIX", SET_WEST, 0x144511u, 0x144511u}, {"PEN", SET_WEST, 0x044111u, 0x044111u},
    {"MPEN", SET_WEST, 0x104441u, 0x104441u}, {"HARM", SET_WEST, 0x414451u, 0x414451u}, {"PHRY", SET_WEST, 0x114445u, 0x114445u},
    {"LYD", SET_WEST, 0x445111u, 0x445111u}, {"LOC", SET_WEST, 0x111445u, 0x111445u}, {"MEL", SET_WEST, 0x444451u, 0x444451u},
    {"BLUES", SET_WEST, 0x105441u, 0x105441u}, {"WHOLE", SET_WEST, 0x111111u, 0x111111u}, {"DIMHW", SET_WEST, 0x145145u, 0x145145u},
    {"DIMWH", SET_WEST, 0x451451u, 0x451451u}, {"T.RST", SET_TURK, 0x444511u, 0x144511u}, {"USSAK", SET_TURK, 0x114451u, 0x114451u},
    {"HUSEY", SET_TURK, 0x144451u, 0x114451u}, {"KURDI", SET_TURK, 0x114445u, 0x114445u}, {"NIHAV", SET_TURK, 0x414451u, 0x114451u},
    {"HICAZ", SET_TURK, 0x144505u, 0x114505u}, {"HCKAR", SET_TURK, 0x414505u, 0x414505u}, {"SUZIN", SET_TURK, 0x414511u, 0x414511u},
    {"KARCI", SET_TURK, 0x141451u, 0x141451u}, {"T.SBA", SET_TURK, 0x114151u, 0x114151u}, {"SEGAH", SET_TURK, 0x114445u, 0x114445u},
    {"HUZZM", SET_TURK, 0x114245u, 0x114245u}, {"RAST", SET_ARAB, 0x244491u, 0x144491u}, {"BAYAT", SET_ARAB, 0x114449u, 0x114449u},
    {"SABA", SET_ARAB, 0x114149u, 0x114149u}, {"SIKAH", SET_ARAB, 0x224889u, 0x222889u}, {"HIJAZ", SET_ARAB, 0x124505u, 0x114505u},
    {"NAHAW", SET_ARAB, 0x414451u, 0x114451u}, {"KURD", SET_ARAB, 0x114445u, 0x114445u}, {"AJAM", SET_ARAB, 0x444511u, 0x444511u},
    {"NIKRZ", SET_ARAB, 0x145051u, 0x145051u}, {"HJKAR", SET_ARAB, 0x414505u, 0x414505u}, {"SUZNK", SET_ARAB, 0x414491u, 0x144491u},
    {"HUZAM", SET_ARAB, 0x228289u, 0x222889u}, {"YAMAN", SET_INDIA, 0x441111u, 0x445111u}, {"BILAW", SET_INDIA, 0x444511u, 0x444511u},
    {"KHMAJ", SET_INDIA, 0x444501u, 0x144511u}, {"KAFI", SET_INDIA, 0x144451u, 0x144451u}, {"ASAVR", SET_INDIA, 0x014411u, 0x114451u},
    {"BHRVI", SET_INDIA, 0x114445u, 0x114445u}, {"BHAIR", SET_INDIA, 0x414505u, 0x414505u}, {"TODI", SET_INDIA, 0x415045u, 0x415045u},
    {"PURVI", SET_INDIA, 0x415105u, 0x415105u}, {"MARWA", SET_INDIA, 0x441105u, 0x441105u}, {"BHUPL", SET_INDIA, 0x044111u, 0x044111u},
    {"MALKS", SET_INDIA, 0x110441u, 0x110441u},
};
static uint32_t spread12(uint32_t m12)
{
    uint32_t i, m = 0;
    for (i = 0; i < 12u; i++)
        m |= ((m12 >> i) & 1u) << (2u * i);
    return m;
}
static uint32_t sc_code(const char *code)
{
    uint32_t i;
    for (i = 0; i < NSCALES; i++)
        if (!strcmp(SCALES[i].code, code))
            return i;
    assert(!"scale code");
    return 0;
}
static uint32_t slots(const uint8_t *a, uint32_t n)
{
    uint32_t m = 0, i;
    for (i = 0; i < n; i++)
        m |= 1u << a[i];
    return m;
}
static void scale_table_test(void)
{
    static const uint8_t PER_SET[NSETS] = {16, 12, 12, 12};
    uint32_t i, k, n[NSETS] = {0, 0, 0, 0};
    track_t *t = &trk[0];
    assert(NSCALES == 52 && NSETS == 4);
    for (i = 0; i < NSCALES; i++) {
        const scale_def_t *s = &SCALES[i];
        uint32_t u = s->asc | s->desc;
        assert(!strcmp(s->code, FROZEN[i].code) && s->set == FROZEN[i].set);          /* the frozen order */
        assert(s->asc == FROZEN[i].asc && s->desc == FROZEN[i].desc);
        assert(strlen(s->code) <= 5 && strlen(s->name) <= 8 && !strcmp(N_SCALE[i], s->code));
        assert((s->asc & 1u) && (s->desc & 1u));                                      /* degree 0 in both forms */
        assert(__builtin_popcount(s->asc) >= 5 && __builtin_popcount(s->asc) <= 12);
        assert(__builtin_popcount(s->desc) >= 5 && __builtin_popcount(s->desc) <= 12);
        assert((s->asc >> 24) == 0 && (s->desc >> 24) == 0);
        for (k = 0; k < i; k++)
            assert(strcmp(SCALES[k].code, s->code));                                  /* unique */
        for (k = 0; k < 24u; k++) {
            assert(s->tune[k] >= -25 && s->tune[k] <= 25);                            /* within half a step */
            assert(((u >> k) & 1u) || !s->tune[k]);                                   /* offsets only on degrees */
            if (s->set == SET_WEST)
                assert(!s->tune[k]);
        }
        n[s->set]++;
        assert(s->home < 24);
        if (i < 16u) {                                                                /* WEST: the 12-tone scales */
            uint32_t m12 = 0, j;
            for (j = 0; j < EXPECTED[i].count; j++)
                m12 |= 1u << EXPECTED[i].notes[j];
            assert(s->asc == spread12(m12) && s->desc == s->asc);
        }
    }
    for (i = 0; i < NSETS; i++)
        assert(n[i] == PER_SET[i]);
    /* the sets are contiguous: WEST 0..15, TURK 16..27, ARAB 28..39, INDIA 40..51 */
    assert(SCALES[15].set == SET_WEST && SCALES[16].set == SET_TURK && SCALES[27].set == SET_TURK &&
           SCALES[28].set == SET_ARAB && SCALES[39].set == SET_ARAB && SCALES[40].set == SET_INDIA);
    /* spec: the quarter-tone and ascending / descending forms, as slots from the root */
    {
        static const uint8_t RA[] = {0, 4, 7, 10, 14, 18, 21}, RD[] = {0, 4, 7, 10, 14, 18, 20}, SA[] = {0, 4, 7, 10, 14, 16, 22};
        const scale_def_t *rast = &SCALES[28], *suz = &SCALES[38], *huz = &SCALES[39], *yam = &SCALES[40];
        assert(!strcmp(rast->code, "RAST") && !strcmp(suz->code, "SUZNK") && !strcmp(huz->code, "HUZAM") && !strcmp(yam->code, "YAMAN"));
        assert(rast->asc == slots(RA, sizeof RA));                                    /* C D E-half-flat F G A B-half-flat */
        assert(rast->desc == slots(RD, sizeof RD));                                   /* ... B-flat going down */
        assert(suz->asc == slots(SA, sizeof SA));                                     /* A-flat, B up */
        assert(suz->desc == slots(RD, sizeof RD));                                    /* A, B-flat down (Rast's upper notes) */
        /* Huzam on E-half-flat: the 4th / 5th degrees are A-flat / B up (9, 15 from the root), A / B-flat down (11, 13) */
        assert(((huz->asc >> 9) & 1u) && ((huz->asc >> 15) & 1u) && !((huz->asc >> 11) & 1u) && !((huz->asc >> 13) & 1u));
        assert(((huz->desc >> 11) & 1u) && ((huz->desc >> 13) & 1u) && !((huz->desc >> 9) & 1u) && !((huz->desc >> 15) & 1u));
        assert(!((yam->asc >> 14) & 1u) && ((yam->desc >> 14) & 1u));                 /* Yaman: no Pa up, Pa down */
    }
    {   /* the ROOT formatter: 24 names, E-half-flat is 7 */
        char val[8];
        const char *unit = "";
        param_format(&TP[P_ROOT], 0, val, &unit);
        assert(!strcmp(val, "C") && TP[P_ROOT].max == 23);
        param_format(&TP[P_ROOT], 7, val, &unit);
        assert(!strcmp(val, "E\x01"));
        param_format(&TP[P_ROOT], 8, val, &unit);
        assert(!strcmp(val, "E"));
        param_format(&TP[P_ROOT], 23, val, &unit);
        assert(!strcmp(val, "C\x01"));
    }
    {   /* task 4.3: what the editor's DESC reply for SCALE is made of (editor.c ed_handle ED_DESC: F_ENUM, names[0..max],
         * at most 64, each ed_str(name, 8) = name + a 0 byte; the frame: 5 header, scope / id / fmt, min / max / def, label,
         * unit, the names, F7, in ed_out[600]). editor.c itself needs the OTA / flash / SysEx plumbing and is not
         * host-compiled; the reply is rebuilt here from the same descriptor. */
        const param_desc_t *d = &TP[P_SCALE];
        uint32_t len = 5 + 3 + 6 + (uint32_t)strlen(d->label) + 1u + (d->unit ? (uint32_t)strlen(d->unit) : 0u) + 1u, i, cnt = 0;
        assert(d->fmt == F_ENUM && d->names && d->min == 0 && d->max == 51);
        for (i = 0; i <= (uint32_t)(d->max - d->min) && i < 64u; i++, cnt++) {
            assert(!strcmp(d->names[i], SCALES[i].code));                             /* the 52 codes, in index order */
            len += (uint32_t)strlen(d->names[i]) + 1u;
        }
        len += 1;
        assert(cnt == 52u && len <= 600u);
        printf("scales: editor DESC for SCALE: %u codes in order, a %u byte reply of 600\n", cnt, len);
    }
    /* design D15: choosing a scale never moves the root */
    t->p[P_ROOT] = 7;
    for (i = 0; i < NSCALES; i++) {
        t->p[P_SCALE] = (int16_t)i;
        assert(t->p[P_ROOT] == 7 && scale_mask(t) == SCALES[i].asc && scale_index(t) == i);
    }
    puts("scales: 52 scales frozen (codes, sets, asc / desc masks), WEST rows, Rast / Suznak / Huzam / Yaman forms, root kept ok");
}

/* maqam (tasks 3.1, 3.2, 3.6): the 24-bit kb_map / chord_notes against the old 12-bit algorithm, on every WEST
 * scale, root, octave and transpose; then the quarter-tone scales */
static uint32_t ref_kb_map(uint32_t mask, int root, int quant, int oct, int trans, uint32_t k)
{
    static const int8_t DEGREE[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    int32_t n = 53 + (int32_t)k, guard = 12, count = 0, degree, o, i;
    if (quant == 1) {
        n += 12 * oct + trans;
        while (guard-- && !((mask >> (uint32_t)((n - root + 120) % 12)) & 1u))
            n--;
        return (uint32_t)clamp(n, 0, 127);
    }
    degree = DEGREE[n % 12];
    if (degree < 0)
        return KB_SILENT;
    degree += (n / 12 - 5) * 7;
    for (i = 0; i < 12; i++)
        count += (mask >> i) & 1u;
    o = degree / count;
    degree %= count;
    if (degree < 0) {
        degree += count;
        o--;
    }
    for (i = 0; i < 12; i++)
        if ((mask >> i) & 1u) {
            if (!degree)
                break;
            degree--;
        }
    n = 60 + root + 12 * o + i;
    return (uint32_t)clamp(n + 12 * oct + trans, 0, 127);
}
static void quarter_key_test(void)
{
    track_t *t = &trk[0];
    uint32_t s, k, c[4], q;
    int root, oct, trans, quant;
    uint8_t ch[4];
    for (quant = 1; quant <= 2; quant++)
        for (s = 0; s < 16u; s++)
            for (root = 0; root < 12; root++)
                for (oct = -3; oct <= 3; oct += 2)
                    for (trans = -24; trans <= 24; trans += 7) {
                        uint32_t mask = mask12_of(SCALES[s].asc);
                        t->p[P_QUANT] = (int16_t)quant;
                        t->p[P_SCALE] = (int16_t)s;
                        t->p[P_ROOT] = (int16_t)(2 * root);
                        t->p[P_TRANS] = (int16_t)trans;
                        t->p[P_CHORD] = 0;
                        song.octave = (int8_t)oct;
                        for (k = 0; k < 27u; k++)
                            assert(kb_map(t, k) == ref_kb_map(mask, root, quant, oct, trans, k));
                    }
    song.octave = 0;
    t->p[P_TRANS] = 0;
    memset(kb_last_key, 0, sizeof kb_last_key);
    /* 3.1 SNAP: the E and the E-flat key both reach E-half-flat in ARAB Rast on C; WEST MAJ unchanged */
    t->p[P_ROOT] = 0;
    t->p[P_QUANT] = 1;
    t->p[P_SCALE] = (int16_t)sc_code("RAST");
    assert(kb_map(t, 11) == (63u | NOTE_Q) && kb_map(t, 10) == (63u | NOTE_Q));       /* E = key 11, D# = key 10 */
    assert(kb_map(t, 12) == 65u && kb_map(t, 9) == 62u && kb_map(t, 7) == 60u);       /* F, D, C */
    assert(kb_map(t, 8) == 60u);                                                      /* C# rounds down to C */
    t->p[P_SCALE] = (int16_t)sc_code("MAJ");
    assert(kb_map(t, 8) == 60u && kb_map(t, 11) == 64u && kb_map(t, 10) == 62u);
    /* 3.2 WHITE: ARAB Bayati on D walks D E-half-flat F G A B-flat C D on C4..C5 */
    t->p[P_QUANT] = 2;
    t->p[P_SCALE] = (int16_t)sc_code("BAYAT");
    t->p[P_ROOT] = 4;
    {
        static const uint8_t KEYS[8] = {7, 9, 11, 12, 14, 16, 18, 19};
        const uint32_t WANT[8] = {62, 63u | NOTE_Q, 65, 67, 69, 70, 72, 74};
        for (k = 0; k < 8u; k++)
            assert(kb_map(t, KEYS[k]) == WANT[k]);
    }
    /* 3.6 chords: TRIAD in ARAB Rast on C = C, E-half-flat, G; 7TH adds B-half-flat; POWER: root, fifth, octave */
    t->p[P_SCALE] = (int16_t)sc_code("RAST");
    t->p[P_ROOT] = 0;
    t->p[P_CHORD] = 1;
    kb_desc_now = 0;
    q = chord_notes(t, 60, ch);
    assert(q == 3 && ch[0] == 60 && ch[1] == (63u | NOTE_Q) && ch[2] == 67);
    t->p[P_CHORD] = 2;
    q = chord_notes(t, 60, ch);
    assert(q == 4 && ch[3] == (70u | NOTE_Q));
    t->p[P_CHORD] = 5;
    q = chord_notes(t, 63u | NOTE_Q, ch);
    assert(q == 3 && ch[0] == (63u | NOTE_Q) && ch[1] == (70u | NOTE_Q) && ch[2] == (75u | NOTE_Q));
    q = chord_notes(t, 60, ch);
    assert(q == 3 && ch[1] == 67 && ch[2] == 72);
    q = chord_notes(t, 127, ch);                                                      /* past the top: dropped */
    assert(q == 1 && ch[0] == 127);
    t->p[P_CHORD] = 0;
    (void)c;
    puts("scales: SNAP / WHITE / chords on the 24-bit masks: WEST identical to the 12-bit algorithm, Rast E, Bayati on D, TRIAD / POWER ok");
}

/* maqam (follow-up to 4.1): set navigation when a set's members are not contiguous. Built against a header
 * generated with one scale added to arab.txt (index 52, ARAB): cc -DSCALE_TEST_53 -I<dir with that scales.h> */
#ifdef SCALE_TEST_53
static void noncontiguous_test(void)
{
    assert(NSCALES == 53 && SCALES[52].set == SET_ARAB && SCALES[39].set == SET_ARAB);
    assert(scale_step(39, 1) == 52 && scale_step(52, 1) == 28 && scale_step(28, -1) == 52 && scale_step(52, -1) == 39);
    assert(scale_next_set(1, 1) == 16 && scale_next_set(20, 1) == 28 && scale_next_set(52, 1) == 40 && scale_next_set(40, 1) == 0);
    assert(scale_set_count(SET_ARAB) == 13 && scale_set_rank(52) == 12 && scale_set_rank(28) == 0);
    assert(scale_step(28, 13) == 28 && scale_step(28, -13) == 28 && scale_step(0, 1) == 1);
    puts("scales: 53 scales, an ARAB scale at index 52: KNOB 2 wraps through 39 -> 52 -> 28, PRESETS lands on 28 ok");
}
#endif

/* maqam (protocol v11): the editor's step bytes: E-half-flat (D#|Q) round-trips in the quarter-tone bits; an older editor keeps them.
 * (A step's bend is a CENT lock: the lock messages, not a step byte.) */
static void editor_step_test(void)
{
    step_t a, b;
    uint8_t w[STEP_WIRE];
    uint32_t i, n;
    memset(&a, 0, sizeof a);
    a.n = 3; a.note[0] = 63u | NOTE_Q; a.note[1] = 64; a.note[2] = 127u | NOTE_Q; a.time = ST_NOTE; a.flags = SF_ACCENT; a.vel = 99;
    a.lvl = 0xB1; a.rat = 0x8E;
    n = step_wire_out(&a, w);
    assert(n == 12u && STEP_WIRE == 12u && w[11] == 5u && w[1] == 63u && w[3] == 127u);        /* Q bits 0 and 2 */
    for (i = 0; i < n; i++)
        assert(w[i] < 128u);                                                                /* every byte is 7-bit safe */
    memset(&b, 0, sizeof b);
    step_wire_in(&b, w, n);
    assert(!memcmp(&a, &b, sizeof a));
    /* an older editor writes 11 bytes (v5) or 8 (v1): a note it did not change keeps its quarter tone */
    memcpy(&b, &a, sizeof b);
    w[5] = ST_NOTE; w[7] = 50;                                                              /* it changes the velocity */
    step_wire_in(&b, w, 11);
    assert(b.note[0] == (63u | NOTE_Q) && b.note[2] == (127u | NOTE_Q) && b.vel == 50 && b.note[1] == 64);
    w[1] = 62;                                                                              /* ... and a note */
    step_wire_in(&b, w, 8);
    assert(b.note[0] == 62 && b.lvl == 0 && b.rat == 0);
    puts("editor: a step with E-half-flat round-trips in the 12 step bytes (protocol v11); an older editor keeps the quarter tones");
}

/* maqam review regression: WHITE keeps one layout in both directions; a degree the played form lacks becomes its
 * nearest degree (the lower one on a tie). Asavari (5 up / 7 down), Yaman (6 / 7), and Rast (7 / 7: unchanged) */
static void white_direction_test(void)
{
    static const uint8_t KEYS[8] = {7, 9, 11, 12, 14, 16, 18, 19};       /* C4 .. C5 */
    track_t *t = &trk[0];
    uint32_t k;
    t->p[P_QUANT] = 2; t->p[P_CHORD] = 0; t->p[P_ROOT] = 0; t->p[P_TRANS] = 0;
    song.octave = 0;
    t->p[P_SCALE] = (int16_t)sc_code("ASAVR");                            /* up C D F G Ab, down C D Eb F G Ab Bb */
    {
        const uint32_t DOWN[8] = {60, 62, 63, 65, 67, 68, 70, 72}, UP[8] = {60, 62, 62, 65, 67, 68, 68, 72};
        for (k = 0; k < 8u; k++)
            assert(kb_quant(t, KEYS[k], 1, 0, 0) == DOWN[k] && kb_quant(t, KEYS[k], 0, 0, 0) == UP[k]);
        assert(kb_quant(t, 8, 0, 0, 0) == P24_SILENT && kb_quant(t, 8, 1, 0, 0) == P24_SILENT);             /* black keys stay silent */
        assert(kb_quant(t, 0, 0, 0, 0) < 60u && kb_quant(t, 26, 1, 0, 0) > 72u);                            /* the layout goes on across octaves */
    }
    t->p[P_SCALE] = (int16_t)sc_code("YAMAN");                            /* up C D E F# A B (no Pa), down adds G */
    {
        const uint32_t DOWN[8] = {60, 62, 64, 66, 67, 69, 71, 72}, UP[8] = {60, 62, 64, 66, 66, 69, 71, 72};
        for (k = 0; k < 8u; k++)
            assert(kb_quant(t, KEYS[k], 1, 0, 0) == DOWN[k] && kb_quant(t, KEYS[k], 0, 0, 0) == UP[k]);
    }
    t->p[P_SCALE] = (int16_t)sc_code("RAST");                             /* 7 / 7: as before */
    assert(kb_quant(t, 18, 0, 0, 0) == (70u | NOTE_Q) && kb_quant(t, 18, 1, 0, 0) == 70u);
    for (k = 0; k < 8u; k++)
        if (KEYS[k] != 18)
            assert(kb_quant(t, KEYS[k], 0, 0, 0) == kb_quant(t, KEYS[k], 1, 0, 0));
    /* chord mode walks the same layout */
    t->p[P_SCALE] = (int16_t)sc_code("ASAVR");
    t->p[P_CHORD] = 1;
    assert(kb_quant(t, 11, 0, 0, 0) == 62u && kb_quant(t, 11, 1, 0, 0) == 63u);
    t->p[P_CHORD] = 0;
    t->p[P_QUANT] = 0;
    puts("scales: WHITE: one layout up and down (Asavari 5/7, Yaman 6/7: a missing degree gives the nearest, lower on a tie), Rast unchanged ok");
}

int main(void)
{
    host_tracks_init();
#ifdef SCALE_TEST_53
    noncontiguous_test();
    return 0;
#endif
    scale_table_test();
    quarter_key_test();
    white_direction_test();
    editor_step_test();
    mapping_test();
    key_events_test();
    return 0;
}
