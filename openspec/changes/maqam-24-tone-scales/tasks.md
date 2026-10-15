# Tasks

## 1. Quarter-tone pitch core

- [x] 1.0 Before each task group in this file: merge upstream SLOOP, run `tests/run_tests.sh` green, and keep every upstream-file edit labelled `/* maqam: */` (design D13). Verify with `git grep -c "maqam:"` listing only intended hooks, and a green test run after the merge.
- [x] 1.1 Create `firmware/src/pitch24.h` (included once from `felucca.c`) with the `note_semi()`, `note_q()` and `note_from_p24()` helpers. Verify with a host unit test in `tests/seq2_test.c` covering 0, 127, 127|Q and round trips.
- [x] 1.2 Extend `trk_note_on` with a `cents` argument, and update every caller in `seq.c`; drums and MIDI in pass 0. Verify with `./build.sh` and `tests/run_tests.sh`; `regress.c` golden hashes must be unchanged.
- [x] 1.3 In `voice_start` and the MONO/UNISON path, compute `pitch16 = semi*16 + q*8 + whole16(cents)`, with `bend_fine` added to the fine sum (design D2). Verify:
  - a `tests/pitch.py` render shows C4|Q at +50 ± 1 c and A4 with +20 c at 20 ± 1 c;
  - golden hashes are unchanged.
- [x] 1.4 Audit every `& 127` / `< 128` / `% 12` applied to a note byte (`seq.c`, `voice.c`, `eng_*.c`, arp, `kb_nt`, `last_note`, `pen_note`). Keep Q where a note is carried, and use `note_semi()` where a semitone is meant. Verify with a seq2 test that runs a Q note through key → record → play → arp, and with a glide test that settles on E-half-flat.
- [x] 1.5 Send `note_semi()` in MIDI out for note-on and note-off (`seq.c` `midi_out_event` calls). Verify with a host test that E-half-flat sends D# on and D# off.

## 2. Scale table generator and 52 scales

- [x] 2.1 Write the scale files `scales/{west,turk,arab,india}.txt` (design D3): pitches from the tonic in cents or ratios, an optional `down` line, a `home` tonic, and a source comment per file (D16). Enter WEST as equal-tempered cents, TURK as Pythagorean ratios (the AEU intervals multiplied from the tonic), ARAB as al-Ṣabbāgh ratios (including the new Suznak and Huzam descending forms), and INDIA as just-intonation ratios. Verify that every row matches the design.md appendix by eye, and in 2.2.
- [x] 2.1b Write `tools/gen_scales.py` (standard library only). It parses the scale files, converts values to cents, rounds them to the 24-grid for the masks with the remainder as `tune[]`, and emits `build/gen/scales.h` with `scale_def_t SCALES[52]` (code, name, set, home, asc, desc, tune). Verify:
  - the self-checks pass: unique codes ≤5 chars, 5..12 degrees, degree 0 in both masks, no tune collisions, no value exactly halfway between two steps, frozen code order;
  - a `--print` mode reproduces the design.md appendix masks and tunes exactly;
  - a file with a shipped scale removed and a colliding file (one slot, two offsets) each make the build fail with the scale named, and a scale added to `arab.txt` gets the next free index in ARAB.
- [x] 2.2 Call `gen_scales.py` from `tools/build.py` next to `gen_tables.py`. Verify that `./build.sh` regenerates `scales.h` and builds.
- [x] 2.3 Replace `SCALE_MASK` / `NSCALES` / `N_SCALE` with `SCALES[]`. Make `P_SCALE` 0..51, with a formatter printing `code`. Make `scale_mask()` return the asc or desc 24-bit mask. Verify with the build and `tests/scale_test.c`.
- [x] 2.4 Make `P_ROOT` 0..23 with a 24-entry `F_NOTE` formatter, and move every `P_ROOT` reader to the 24 grid (`kb_map`, `scale_keys`, SCL screen, SLICE base = ROOT/2). Verify that `scale_test.c` passes with roots ×2 and a SLICE key-map test is unchanged.
- [x] 2.5 Apply `tune[(p24-root) mod 24]` plus the step bend at note-on, clamped to ±100 (design D4). Verify these renders:
  - TURK Rast third on C at 385 ± 3 c;
  - INDIA Bhupali Ga at 386 ± 3 c;
  - TURK Segah ≠ Kürdi on the same degree;
  - ARAB Rast third on C at 357 ± 3 c in TRADITIONAL;
  - WEST unchanged (golden hashes).
- [x] 2.5a Add the global `G_INTON` setting {EQUAL, TRADITIONAL} (design D14): append it to the `G_*` list, default it to TRADITIONAL in new projects and in the `FUN5` importer, and gate `tune[]` on it at note-on. While SCL is held, the ALGORITHM encoder toggles it, and the SCL subtitle shows `equal` / `trad`. Verify with host tests:
  - TURK Rast third on C renders at 385 ± 3 c in TRADITIONAL and 400 ± 1 c in EQUAL;
  - a WEST render is identical in both;
  - a FUN4 project loads as TRADITIONAL;
  - SCL + ALGORITHM toggles the setting, and the subtitle changes.
- [x] 2.6 Extend `tests/scale_test.c`. Keep the existing WEST EXPECTED rows, and add the rows for every TURK/ARAB/INDIA scale (asc and desc, including ARAB Suznak and Huzam descending with A and B-flat), the frozen list of 52 codes, and a check that selecting a scale never changes `P_ROOT` (design D15). Verify with `tests/run_tests.sh`.

## 3. Microtonal key input

- [x] 3.1 `kb_map` SNAP: round down from `2*semi + 1` in the 24-bit mask. Verify:
  - the `scale_test.c` WEST SNAP rows are unchanged;
  - in ARAB Rast on C, the E and E♭ keys give E-half-flat.
- [x] 3.2 `kb_map` WHITE: walk the degrees of the 24-bit mask. Verify that ARAB Bayati on D gives D, E-half-flat, F, G, A, B♭, C, D on C4..C5.
- [x] 3.3 Add a per-track `kb_last_key` and direction-based asc/desc selection (design D5). Verify with a seq2 test: ARAB Rast going up gives B-half-flat, coming down gives B♭, and the recorded step replays B♭.
- [x] 3.4 OCT− / OCT+: change the octave on release, only if no key was pressed while held. Holding them lowers / raises a synth key by a quarter tone; the drum track is unchanged. Verify with a `ui_pages_test.c` or hostsim test (CHR, OCT− + E → E-half-flat; press and release alone → octave ±1; both buttons → 0) and on the device.
- [x] 3.5 SCL layer root key + OCT− / OCT+ sets a half-flat / half-sharp root. Verify that the host test SCL + OCT− + E gives root 7 (E-half-flat).
- [x] 3.6 `chord_notes` on the 24-bit mask, with POWER = {0, 14, 24} quarter tones. Verify that TRIAD in ARAB Rast on C gives C, E-half-flat, G, and that the WEST chord tests are unchanged.
- [x] 3.7 `scale_keys()` and the SCL screen tiles: a key is lit when its semitone or the quarter tone above it is in the scale. Verify that in ARAB Rast the E key is lit (`ui_pages_test.c`).

## 4. Scale set selection UI

- [x] 4.1 In the SCL layer, EN_PRESET cycles WEST → TURK → ARAB → INDIA and selects the first scale of the set, and KNOB2 wraps within the current set (design D8). Verify with host tests: WEST MAJ + PRESETS +1 → index 16; last ARAB + KNOB2 +1 → index 28.
- [x] 4.2 The SCL screen subtitle shows the set, the name and the intonation (`key: E↓ arab sikah trad`), plus `home: <note>` for non-WEST scales (design D15), and the position bar shows the place within the set. Verify on the device or in the emulator, and in a `ui_pages_test.c` string check (ARAB Ajam on C shows `home: B♭` and the root stays C).
- [x] 4.3 Editor DESC sends the 52 codes. Verify that a host test of the DESC reply fits in `ed_out` and lists 52 names in order.
- [x] 4.4 Update `SLOOP.md` (scale sets, PRESETS while SCL is held, ALGORITHM for EQUAL / TRADITIONAL, any scale on any root with its home hint, the list of scales) and the README feature list. Add a short `scales/README.md` explaining how to add a scale: the cents or ratio syntax, `down` lines, the append-only rule and source comments. Verify that the docs name all 52 codes, the controls match the firmware, and following `scales/README.md` adds a test scale that builds.

## 5. Step cent bend and persistence

- [x] 5.1 Add `int8_t cents` to `step_t` and `uint8_t rsv` to `dstep_t`, and update the size assert to 11. Verify that `./build.sh` passes the static asserts.
- [x] 5.2 Sequencer playback passes `step.cents` to `trk_note_on`. Verify that a step with A4 at −12 renders 12 ± 1 c flat, and that a chord step applies the bend to every note.
- [x] 5.3 Step page: add a CENT knob (−63..+63) and keep Q when NOTE turns by semitones; with OCT− / OCT+ held, NOTE moves by quarter tones. The tile shows `±Nc`. Pattern transpose keeps Q. Verify with host tests (E-half-flat +2 st → F#-half-flat; E + OCT− nudge → E-half-flat) and on the device.
- [x] 5.4 Project `FUN5`: new magic, and a FUN4 importer (cents = 0, ROOT ×2, scale unchanged). `proj_apply` keeps Q and clamps cents. Verify in `tests/project_test.c`:
  - a FUN4 blob with root D / MIN imports to root 4 / MIN with cents 0;
  - Q and cents (−63, 0, +63) round-trip;
  - −100 is clamped to −63;
  - the `.noinit` and `ST_PAYLOAD_MAX` asserts pass.
- [x] 5.5 User presets keep Q in their pattern notes, at UP_VER 1. Verify with an `upreset_test.c` round trip of B-half-flat.
- [x] 5.6 Document the step bend, the half-flat / half-sharp modifier, the change of OCT− / OCT+ to act on release, and the format change (backup before updating) in `SLOOP.md` and the release notes. Verify that the docs match the behavior in the device checks.

## 6. Display

- [x] 6.1 Add half-flat and half-sharp glyphs to the `gfx.c` font, and a 24-entry note-name table. Verify with a screenshot on the device or in the host render of a step page showing `E↓4` and `F‡3`.
- [x] 6.2 `note_name()` takes a note byte with Q, and all on-screen note names use it (step page, SEQ/SCL tiles, KEY toast). Verify with `ui_pages_test.c` string checks for E-half-flat and an unchanged `C#4`.

## 7. Web editor

- [x] 7.1 Protocol: STEP_GET / SET and TRACK_STEP carry a Q bit per note and a cents byte (+64 offset). Bump the version in `web/EDITOR_PROTOCOL.md`, and update `editor.c` `ed_step_in` / out. Verify with a host test round-tripping a step with E-half-flat and +7 c.
- [x] 7.2 `web/editor.html`: `noteName` / `parseNote` handle `d` / `+` (`Ed4`, `F+3`), add a cents column, and hide it on older firmware. Mirror to `docs/webapp/editor/index.html`. Verify that `node web/test_web.mjs` passes with new parse/print cases.
- [x] 7.3 Mock device: the TP table lists 52 scales and ROOT 0..23; update the `web/test_web.mjs:71-75` expectations. Verify that `node web/test_web.mjs` passes.

## 9. Merge onto SLOOP 2.6 (design D17)

Tasks 5.1–5.4, 6.1, 7.1 and 7.2 were done on 2.3 and are partly superseded by this group: the step byte, STEP 2, FUN5 and the cents step byte are gone.

- [x] 9.1 Merge upstream SLOOP 2.6 (`d315ad8`) into the branch and resolve the 19 conflicted files. Verify: no conflict markers; 182 golden renders with 0 changed against upstream's `golden.txt` (12-tone sound equals 2.6); the full host suite passes except the CPU-budget entry that clean 2.6 also fails on this machine.
- [x] 9.2 Make step bends CENT parameter locks: lockable `P_CENT` on SEL 2, upstream's lock gesture, `step_t` back to 10 bytes. Verify with the `seq2_test` and `ui_pages_test` CENT-lock checks (the lock plays during its step, the next step is back in tune, chords bend).
- [x] 9.3 Store projects as upstream FUN6 plus the MQ extension block, with editor protocol 11. Verify with `project_test` (FUN6 size and magic, 2.6 projects load with defaults, a damaged block falls back to defaults, a full round trip) and `node web/test_web.mjs` (qbits, CENT locks, a proto-10 device).
- [ ] 9.4 Rebuild upstream's audio preview (`tools/build_preview.py`, needs `emcc`) so `sloop-preview.wasm` has CENT and quarter tones. Verify with `node tests/preview_test.mjs`.
- [x] 9.5 Re-render the `SCALES.md` screenshots on the merged firmware (SEL naming, CENT lock instead of STEP 2). Verify by viewing them.
- [ ] 9.6 Mention the scales and CENT locks in upstream's `GUIDE.md` and the French quick start. Verify that the docs match the controls.

## 10. Colours and predictable SNAP

- [x] 10.1 Track colours: green, red, yellow, pink (OKLCH-balanced; pink for Hind Rajab) in `TE_COL` / `TE_DIM`, with every hard-coded use of the old four routed through them. Verify with rendered screens and the golden renders (sound unchanged).
- [x] 10.2 SNAP follows the direction (design D6), and WHITE gaps follow the same rule. Verify with `scale_test` (Rast E♭/E, Asavari up/down, WEST C# up/down, a two-degree window) and `white_direction_test`.
- [x] 10.3 Direction resets to ascending after 1 s without a key on the track, and the SEL subtitle shows ↑/↓ for two-form scales. Verify with the `seq2_test` pause test and `ui_pages_test` (arrow present only for two-form scales, fits the width).
- [x] 10.5 OCT− / OCT+ with keys held play them again a quarter tone off (and back on release); a double tap changes the octave, a single tap nothing (design D7, revised). Verify with `ui_pages_test` (held E → E-half-flat → E, double tap ±1, single taps and slow taps none, a key held blocks the octave, the drum track unchanged).
- [x] 10.6 The sustain pedal's waiting notes cover 256 note bytes, so a quarter-tone note under the pedal ends. Verify with the host suite.
- [x] 10.7 Chord modes for microtonal scales (design D18): 8VA, QUART, DRONE added; SUS4 and QUART exact off WEST, diatonic on WEST; CHORD+ modifiers limited to the third-based modes; SEL tiles `8` / `q`, DRONE named by the key's note. Verify with `scale_test` (Rast 8VA / SUS4 / QUART on E-half-flat, Saba QUART exact, DRONE below and on the tonic and on a quarter-tone root, WEST SUS4 / QUART diatonic) and the editor's CHORD list (`test_web.mjs`).
- [x] 10.8 Review fixes: OCT re-presses held keys in the order they were played (MONO keeps the last one); under the arp the notes change in place (no restart, nothing latched twice), and the arp keeps OCT's quarter tone when it re-pitches (`held_dt` from the key's plain note); a learned CC sweeps upstream's ranges for ROOT (12 semitone roots), SCALE (the 16 WEST scales) and CHORD (OFF..POWER), so a controller set up on SLOOP picks the same values. Verify with `ui_pages_test` (order, arp) and `scale_test` (CC ranges).
- [x] 10.4 Re-render the `SCALES.md` screenshots (plus an Asavari ↓ shot), update the SNAP/WHITE/direction text in `SCALES.md` and `SLOOP.md`, and rebuild the site and emulator. Verify by viewing the images and the site.

## 8. Integration checks

- [ ] 8.1 Full `./build.sh` and `tests/run_tests.sh`. Verify that everything is green, the golden hashes are unchanged, `target_budget.py` is within budget, and the pool keeps at least 8 KiB spare.
- [ ] 8.2 On the FM-1:
  - ARAB Rast WHITE up and down (B-half-flat / B♭);
  - TURK Segah vs Kürdi audibly different;
  - record and replay with bends (CENT locks);
  - SEL + PRESETS set cycling;
  - a SLOOP 2.6 project and backup load and sound the same; a fork project opens on SLOOP 2.6 firmware in 12-tone.

  Verify with a written device test log in the change folder.
