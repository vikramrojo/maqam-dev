# Tasks

## 1. Quarter-tone pitch core

- [ ] 1.0 Before each task group in this file: merge upstream SLOOP, run `tests/run_tests.sh` green, and keep every upstream-file edit labelled `/* maqam: */` (design D13). Verify with `git grep -c "maqam:"` listing only intended hooks, and a green test run after the merge.
- [ ] 1.1 Create `firmware/src/pitch24.h` (included once from `felucca.c`) with the `note_semi()`, `note_q()` and `note_from_p24()` helpers. Verify with a host unit test in `tests/seq2_test.c` covering 0, 127, 127|Q and round trips.
- [ ] 1.2 Extend `trk_note_on` with a `cents` argument, and update every caller in `seq.c`; drums and MIDI in pass 0. Verify with `./build.sh` and `tests/run_tests.sh`; `regress.c` golden hashes must be unchanged.
- [ ] 1.3 In `voice_start` and the MONO/UNISON path, compute `pitch16 = semi*16 + q*8 + whole16(cents)`, with `bend_fine` added to the fine sum (design D2). Verify:
  - a `tests/pitch.py` render shows C4|Q at +50 ± 1 c and A4 with +20 c at 20 ± 1 c;
  - golden hashes are unchanged.
- [ ] 1.4 Audit every `& 127` / `< 128` / `% 12` applied to a note byte (`seq.c`, `voice.c`, `eng_*.c`, arp, `kb_nt`, `last_note`, `pen_note`). Keep Q where a note is carried, and use `note_semi()` where a semitone is meant. Verify with a seq2 test that runs a Q note through key → record → play → arp, and with a glide test that settles on E-half-flat.
- [ ] 1.5 Send `note_semi()` in MIDI out for note-on and note-off (`seq.c` `midi_out_event` calls). Verify with a host test that E-half-flat sends D# on and D# off.

## 2. Scale table generator and 52 scales

- [ ] 2.1 Write `tools/gen_scales.py` (standard library only) with the WEST masks (spread from 12 bits), ARAB quarter-tone degrees, TURK comma steps and INDIA swara strings with JI ratios, as listed in the design.md appendix. It emits `build/gen/scales.h` with `scale_def_t SCALES[52]`. Verify: the script's self-checks pass (unique codes ≤5 chars, 5..12 degrees, degree 0 in both masks, no tune collisions, frozen code order), and the printed masks match the design.md appendix.
- [ ] 2.2 Call `gen_scales.py` from `tools/build.py` next to `gen_tables.py`. Verify that `./build.sh` regenerates `scales.h` and builds.
- [ ] 2.3 Replace `SCALE_MASK` / `NSCALES` / `N_SCALE` with `SCALES[]`. Make `P_SCALE` 0..51, with a formatter printing `code`. Make `scale_mask()` return the asc or desc 24-bit mask. Verify with the build and `tests/scale_test.c`.
- [ ] 2.4 Make `P_ROOT` 0..23 with a 24-entry `F_NOTE` formatter, and move every `P_ROOT` reader to the 24 grid (`kb_map`, `scale_keys`, SCL screen, SLICE base = ROOT/2). Verify that `scale_test.c` passes with roots ×2 and a SLICE key-map test is unchanged.
- [ ] 2.5 Apply `tune[(p24-root) mod 24]` plus the step bend at note-on, clamped to ±100 (design D4). Verify these renders:
  - TURK Rast third on C at 385 ± 3 c;
  - INDIA Bhupali Ga at 386 ± 3 c;
  - TURK Segah ≠ Kürdi on the same degree;
  - WEST unchanged (golden hashes).
- [ ] 2.6 Extend `tests/scale_test.c`. Keep the existing WEST EXPECTED rows, and add the rows for every TURK/ARAB/INDIA scale (asc and desc) and the frozen list of 52 codes. Verify with `tests/run_tests.sh`.

## 3. Microtonal key input

- [ ] 3.1 `kb_map` SNAP: round down from `2*semi + 1` in the 24-bit mask. Verify:
  - the `scale_test.c` WEST SNAP rows are unchanged;
  - in ARAB Rast on C, the E and E♭ keys give E-half-flat.
- [ ] 3.2 `kb_map` WHITE: walk the degrees of the 24-bit mask. Verify that ARAB Bayati on D gives D, E-half-flat, F, G, A, B♭, C, D on C4..C5.
- [ ] 3.3 Add a per-track `kb_last_key` and direction-based asc/desc selection (design D5). Verify with a seq2 test: ARAB Rast going up gives B-half-flat, coming down gives B♭, and the recorded step replays B♭.
- [ ] 3.4 OCT− / OCT+: change the octave on release, only if no key was pressed while held. Holding them lowers / raises a synth key by a quarter tone; the drum track is unchanged. Verify with a `ui_pages_test.c` or hostsim test (CHR, OCT− + E → E-half-flat; press and release alone → octave ±1; both buttons → 0) and on the device.
- [ ] 3.5 SCL layer root key + OCT− / OCT+ sets a half-flat / half-sharp root. Verify that the host test SCL + OCT− + E gives root 7 (E-half-flat).
- [ ] 3.6 `chord_notes` on the 24-bit mask, with POWER = {0, 14, 24} quarter tones. Verify that TRIAD in ARAB Rast on C gives C, E-half-flat, G, and that the WEST chord tests are unchanged.
- [ ] 3.7 `scale_keys()` and the SCL screen tiles: a key is lit when its semitone or the quarter tone above it is in the scale. Verify that in ARAB Rast the E key is lit (`ui_pages_test.c`).

## 4. Scale set selection UI

- [ ] 4.1 In the SCL layer, EN_PRESET cycles WEST → TURK → ARAB → INDIA and selects the first scale of the set, and KNOB2 wraps within the current set (design D8). Verify with host tests: WEST MAJ + PRESETS +1 → index 16; last ARAB + KNOB2 +1 → index 28.
- [ ] 4.2 The SCL screen subtitle shows the set and name (`key: E↓ arab sikah`), and the position bar shows the place within the set. Verify on the device and in a `ui_pages_test.c` string check.
- [ ] 4.3 Editor DESC sends the 52 codes. Verify that a host test of the DESC reply fits in `ed_out` and lists 52 names in order.
- [ ] 4.4 Update `SLOOP.md` (scale sets, PRESETS while SCL is held, the list of scales) and the README feature list. Verify that the docs name all 52 codes and the controls match the firmware.

## 5. Step cent bend and persistence

- [ ] 5.1 Add `int8_t cents` to `step_t` and `uint8_t rsv` to `dstep_t`, and update the size assert to 11. Verify that `./build.sh` passes the static asserts.
- [ ] 5.2 Sequencer playback passes `step.cents` to `trk_note_on`. Verify that a step with A4 at −12 renders 12 ± 1 c flat, and that a chord step applies the bend to every note.
- [ ] 5.3 Step page: add a CENT knob (−63..+63) and keep Q when NOTE turns by semitones; with OCT− / OCT+ held, NOTE moves by quarter tones. The tile shows `±Nc`. Pattern transpose keeps Q. Verify with host tests (E-half-flat +2 st → F#-half-flat; E + OCT− nudge → E-half-flat) and on the device.
- [ ] 5.4 Project `FUN5`: new magic, and a FUN4 importer (cents = 0, ROOT ×2, scale unchanged). `proj_apply` keeps Q and clamps cents. Verify in `tests/project_test.c`:
  - a FUN4 blob with root D / MIN imports to root 4 / MIN with cents 0;
  - Q and cents (−63, 0, +63) round-trip;
  - −100 is clamped to −63;
  - the `.noinit` and `ST_PAYLOAD_MAX` asserts pass.
- [ ] 5.5 User presets keep Q in their pattern notes, at UP_VER 1. Verify with an `upreset_test.c` round trip of B-half-flat.
- [ ] 5.6 Document the step bend, the half-flat / half-sharp modifier, the change of OCT− / OCT+ to act on release, and the format change (backup before updating) in `SLOOP.md` and the release notes. Verify that the docs match the behavior in the device checks.

## 6. Display

- [ ] 6.1 Add half-flat and half-sharp glyphs to the `gfx.c` font, and a 24-entry note-name table. Verify with a screenshot on the device or in the host render of a step page showing `E↓4` and `F‡3`.
- [ ] 6.2 `note_name()` takes a note byte with Q, and all on-screen note names use it (step page, SEQ/SCL tiles, KEY toast). Verify with `ui_pages_test.c` string checks for E-half-flat and an unchanged `C#4`.

## 7. Web editor

- [ ] 7.1 Protocol: STEP_GET / SET and TRACK_STEP carry a Q bit per note and a cents byte (+64 offset). Bump the version in `web/EDITOR_PROTOCOL.md`, and update `editor.c` `ed_step_in` / out. Verify with a host test round-tripping a step with E-half-flat and +7 c.
- [ ] 7.2 `web/editor.html`: `noteName` / `parseNote` handle `d` / `+` (`Ed4`, `F+3`), add a cents column, and hide it on older firmware. Mirror to `docs/webapp/editor/index.html`. Verify that `node web/test_web.mjs` passes with new parse/print cases.
- [ ] 7.3 Mock device: the TP table lists 52 scales and ROOT 0..23; update the `web/test_web.mjs:71-75` expectations. Verify that `node web/test_web.mjs` passes.

## 8. Integration checks

- [ ] 8.1 Full `./build.sh` and `tests/run_tests.sh`. Verify that everything is green, the golden hashes are unchanged, `target_budget.py` is within budget, and the pool keeps at least 8 KiB spare.
- [ ] 8.2 On the FM-1:
  - ARAB Rast WHITE up and down (B-half-flat / B♭);
  - TURK Segah vs Kürdi audibly different;
  - record and replay with bends;
  - SCL + PRESETS set cycling;
  - a SLOOP 2.3 project and backup load and sound the same.

  Verify with a written device test log in the change folder.
