# Tasks

## 1. Forward-compatibility guard rails

- [ ] 1.1 Add a test that builds with and without `MAQAM_PRESS` and checks that the build without the flag is byte-identical to the build before this change (SHA-256 of `build/felucca.fwsc`). Verify in CI.
- [ ] 1.2 MQ block bytes 9..10 for AFT, with byte 8's check and the version unchanged. Verify with `project_test.c`: round trip, an MQ v1 block without the field (AFT OFF), a damaged field (AFT OFF, the rest loaded), and an upstream FUN6 project (AFT OFF).

## 2. HAL (behind MAQAM_PRESS)

- [ ] 2.1 `firmware/hal/fm1_press.h`: deferred SARADC conversions in the scan tick, the row drive and release, landing and `fm1_press[27]`, after fm1-velocity (credit Charles Vestal, GPL-3.0-only). One `FM1_INPUT_SENSE` hook in `fm1_input.h`. Verify with a host test of the pressure state machine on recorded samples.
- [ ] 2.2 The MASTER pot and battery read from scan slots when the flag is on. Verify on the device that MASTER and the battery icon still work.
- [ ] 2.3 The tick's time with sensing and the dim-LED pulses: measure on the device and add an ISR entry to `target_budget.py`.

## 3. Bend

- [ ] 3.1 `voice_t.at_q`, smoothed, added next to `bendq`; reset at `voice_start`. Verify that the golden renders are unchanged with AFT OFF, and that a host test with a forced pressure measures 50 ± 3 c at full pressure with 50c.
- [ ] 3.2 `seq.c`: every 5 ms, held keys → their voices (chords together; MONO / LEGATO the sounding key); OCT re-press keeps the landing. Verify with `seq2_test.c`.
- [ ] 3.3 G_AFT (OFF / 25c / 50c / 100c) on GLO, in the editor's globals, and MIDI-learnable. Verify with `ui_pages_test.c` and `test_web.mjs`.
- [ ] 3.4 Poly key pressure out with AFT on. Verify with `seq2_test.c` (no messages with AFT OFF).

## 4. Device checks and docs

- [ ] 4.1 On the FM-1: every key still plays (no stuck keys), pressure bends F3..C5, OCT− at power-on still gives USB rescue, and the UBOOT combo still works.
- [ ] 4.2 Document AFT in `SCALES.md`, `SLOOP.md` and the site guide, including which keys respond.
