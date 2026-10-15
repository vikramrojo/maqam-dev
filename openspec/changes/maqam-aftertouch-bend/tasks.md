# Tasks

## 1. Forward-compatibility guard rails

- [x] 1.1 A host test that the default build compiles none of the pressure code (the preprocessed `felucca.c` without `MAQAM_PRESS` has no `fm1_press` / `press_block` / `press_q` / `G_AFT` / sense hooks; with it, it has them). Verify in `tests/run_tests.sh`.
- [x] 1.2 MQ block bytes 9..10 for AFT, with byte 8's check and the version unchanged. Verify with `project_test.c` built with `MAQAM_PRESS=1`: round trip, the v1 block still valid for an older reader, a damaged field (AFT OFF, the rest loaded), a SLOOP 2.6 project (AFT OFF), a song section leaving it.

## 2. HAL (behind MAQAM_PRESS)

- [x] 2.1 `firmware/hal/fm1_press.h`: deferred SARADC conversions in the scan tick, the row drive and release, the landing and `fm1_press[27]`, after fm1-velocity (credited). Two hooks in `fm1_input.h`. Verify the arithmetic with `tests/press_test.c` (landing, half, full, eased off).
- [x] 2.2 The MASTER pot and the battery from scan slots with the flag on (`main.c`). Verify on the device (4.1).
- [ ] 2.3 The tick's time with sensing and the dim-LED pulses: measure on the device; add an ISR entry to `target_budget.py` if it is close.

## 3. Bend

- [x] 3.1 `press_q`, smoothed, added next to `bendq`; a new note from 0. Verify with `press_test.c` (full: ~1024 / 4096 semitone = 25 c, half ~12 c, back to 0, AFT OFF none) and the golden renders unchanged in the default build.
- [x] 3.2 `press.c`: held keys → their voices (chords together, keys on their own pressure); OCT re-press keeps the landing (it is the HAL's, per physical key). Verify with `press_test.c`.
- [x] 3.3 G_AFT (OFF / ON) on the GLO page PRESS, in the editor's globals through the description. Verify with `press_test.c` and `test_web.mjs` (the default build unchanged).
- [x] 3.4 Poly key pressure out with AFT ON, none with AFT OFF. Verify with `press_test.c`.
- [x] 3.5 CI builds the test package `maqam-press-firmware-<sha>` (About: `MAQAM 2.6 AFT`) after the default one.

## 4. Device checks and docs

- [ ] 4.1 On the FM-1 with the test package (see `qa.md`): every key still plays (no stuck or doubled keys), MASTER and the battery work, pressure bends F3..C5 up to ~25 c and nothing above C5, OCT− at power-on still gives USB rescue, and the UBOOT combo still works.
- [ ] 4.2 Decide from 4.1: the depth, the direction, the LED; then document AFT in `SCALES.md`, `SLOOP.md` and the site guide, and decide whether it becomes the default build.
