# Proposal

## Why

SLOOP is built entirely on 12-tone equal temperament. Keys, steps, scales and voices all carry MIDI note numbers, and scales are 12-bit masks. That makes Arabic maqam, Turkish makam and Indian raga music impossible to play faithfully. These traditions need quarter tones (half-flats and half-sharps), scales whose ascending and descending forms differ, and small intonation deviations that no equal grid captures. The voice engine already resolves pitch to 1/16 semitone, and the step note byte has an unused top bit. A 24-tone pitch core can therefore be added almost for free, without giving up the 12-tone behavior existing users rely on.

## What Changes

- **24-tone pitch core.** Note bytes gain a quarter-tone flag (bit 7), so pitch = semitone + optional quarter tone. Voices render quarter tones exactly. 12-TET notes stay byte-identical, so existing Western sound is unchanged.
- **Root in quarter tones.** The scale root (`P_ROOT`) changes from 0..11 semitones to 0..23 quarter tones, so a scale can be rooted on E half-flat, for example. **BREAKING** for the project format: projects migrate automatically (the root is doubled).
- **Four scale sets, 52 scales.** The scale table is rebuilt as 24-tone ascending/descending masks plus a per-degree cent tuning table, grouped into four sets:
  - WEST: the 16 existing scales, unchanged and at the same indices.
  - TURK: 12 makams tuned to the 53-comma system.
  - ARAB: 12 maqamat on the 24-tone grid.
  - INDIA: 12 ragas in just intonation.
- **Ascending/descending by direction of play.** A key lower than the previous one uses the scale's descending form, otherwise the ascending form. The resolved note is what gets recorded.
- **Quarter-tone key input.**
  - SNAP: each key covers its own semitone and the quarter tone above it, then rounds down into the scale.
  - WHITE: the white keys walk the scale's degrees.
  - Holding OCT− / OCT+ while pressing a key on a synth track gives a half-flat / half-sharp, in chromatic mode and when choosing the root.
  - The octave shift moves from button press to button release (only when no key was played while the button was held).
- **Scale set selection.** While SCL is held, the PRESETS encoder cycles the four sets and KNOB2 scrolls within the current set.
- **Per-step cent bend.** Every synth step stores a signed cent offset (−63..+63), edited with a CENT knob on the step page. **BREAKING** for the project format: `FUN4` becomes `FUN5`, with an importer, and the step grows from 10 to 11 bytes.
- **Display.** Note names on screen show half-flat and half-sharp glyphs, and cents when they are non-zero.
- **Web editor.**
  - Note names parse and print quarter tones (`Ed4`, `E+4`).
  - The step SysEx messages carry cents (protocol version bump).
  - The scale list grows to 52 names.
- **Out of scope:** microtonal MIDI out (notes are sent at their semitone, without the quarter tone), microtonal MIDI in, and pytheory as a dependency (it served as research only).

## Capabilities

### New Capabilities
- `quarter-tone-pitch`: How notes are represented and sounded on a 24-tone grid. Covers the quarter-tone flag, the root in quarter tones, cent offsets reaching the oscillator, the 12-TET compatibility guarantee, and MIDI out rounding.
- `scale-sets`: The table of 52 scales in four sets: 24-tone ascending/descending masks, per-degree intonation, append-only indices, names, set selection in the SCL layer, and the list exposed to the web editor.
- `microtonal-key-input`: How the 27 physical keys produce quarter tones. Covers SNAP and WHITE quantizing on the 24 grid, choosing ascending or descending by direction, the OCT− / OCT+ half-flat / half-sharp modifier, and chords built from 24-tone scales.
- `step-cent-bend`: Microtonal sequencer steps. Covers a per-step cent offset, quarter tones kept through step editing and transposition, the project format and its migration, and step exchange with the web editor.

### Modified Capabilities
<!-- none: the project has no specs yet -->

## Impact

- **Firmware:**
  - Pitch path: `core.h` (`step_t`, `dstep_t`, `voice_t`), `voice.c` (`trk_note_on`, `voice_start`, block pitch).
  - Scales and keys: `seq.c` (`SCALE_MASK` → `SCALES`, `kb_map`, `chord_notes`, MIDI out), `params.c` (`N_SCALE`, `N_NOTE`, `P_ROOT`).
  - Persistence: `project.c` (`FUN5` and the `FUN4` importer).
  - UI: `ui_input.c` (octave buttons, SCL encoders, step knobs, key lights), `ui_layers.c` (SCL layer and screen, transpose), `ui_draw.c` / `ui.c` (note names), `gfx.c` (two glyphs).
  - Other: `upreset.c` (keeps Q), `editor.c` (step fields, DESC).
- **Build:** a new `tools/gen_scales.py` (standard library only) emits `build/gen/scales.h`. `tools/build.py` runs it.
- **Memory:** +256 B per project (still within the `.noinit` and `ST_PAYLOAD_MAX` limits), about 2 KB of flash for the scale table, and no new RAM tables.
- **Web:**
  - `web/editor.html` and its copy `docs/webapp/editor/index.html`.
  - `web/EDITOR_PROTOCOL.md` (step cents, new version).
  - `web/test_web.mjs`.
- **Tests:** `tests/scale_test.c`, `project_test.c`, `seq2_test.c`, `regress.c` / `golden.txt` (the Western hashes must not change).
- **Docs:** `SLOOP.md` and `README.md` (the scale sets, quarter-tone input, cents).
- **Compatibility:**
  - Old projects load (migrated).
  - Older firmware cannot read `FUN5` projects.
  - The MIDI out note numbers are unchanged.
