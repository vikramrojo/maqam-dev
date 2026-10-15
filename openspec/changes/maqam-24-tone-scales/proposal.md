# Proposal

## Why

*Rebased onto SLOOP 2.6 (October 2026): see design D17 for what changed in the merge.*

SLOOP is built entirely on 12-tone equal temperament. Keys, steps, scales and voices all carry MIDI note numbers, and scales are 12-bit masks. That makes Arabic maqam, Turkish makam and Indian raga music impossible to play faithfully. These traditions need quarter tones (half-flats and half-sharps), scales whose ascending and descending forms differ, and small intonation deviations that no equal grid captures. The voice engine already resolves pitch to 1/16 semitone, and the step note byte has an unused top bit. A 24-tone pitch core can therefore be added almost for free, without giving up the 12-tone behavior existing users rely on.

## What Changes

- **24-tone pitch core.** Note bytes gain a quarter-tone flag (bit 7), so pitch = semitone + optional quarter tone. Voices render quarter tones exactly. 12-TET notes stay byte-identical, so existing Western sound is unchanged.
- **Root in quarter tones.** The scale root (`P_ROOT`) changes from 0..11 semitones to 0..23 quarter tones, so a scale can be rooted on E half-flat, for example. **BREAKING** for the project format: projects migrate automatically (the root is doubled).
- **Four scale sets, 52 scales.** The scale table is rebuilt as 24-tone ascending/descending masks plus a per-degree cent tuning table, grouped into four sets:
  - WEST: the 16 existing scales, unchanged and at the same indices.
  - TURK: 12 makams in the Arel-Ezgi-Uzdilek system (Pythagorean ratios, named by their size in 53-comma steps).
  - ARAB: 12 maqamat on the 24-tone grid, with traditional intonation from al-Ṣabbāgh (1954).
  - INDIA: 12 ragas in just intonation.
- **An introduction to microtonality, not an archive.** Any scale can be played from any root. Each non-Western scale shows its traditional home tonic as a hint, but choosing a scale never moves the root.
- **Intonation switch.** A global EQUAL / TRADITIONAL setting. EQUAL plays every scale on the plain 24-tone grid. TRADITIONAL adds each scale's small per-degree offsets (at most ±22 c). Players can hear the difference on one held note. Western scales sound the same either way.
- **Scales are data.** Scales live in plain text files (`scales/*.txt`), one per set. Pitches are written from the tonic in cents or as ratios, the same syntax Scala `.scl` files use. Adding a scale means adding a few lines; a build-time generator turns them into the firmware tables and checks them.
- **Ascending/descending by direction of play.** A key lower than the previous one uses the scale's descending form, otherwise the ascending form. The resolved note is what gets recorded.
- **Quarter-tone key input.**
  - SNAP: each key covers its own semitone and the quarter tone above it, then rounds down into the scale.
  - WHITE: the white keys walk the scale's degrees.
  - Holding OCT− / OCT+ while pressing a key on a synth track gives a half-flat / half-sharp, in chromatic mode and when choosing the root.
  - The octave shift moves from button press to button release (only when no key was played while the button was held).
- **Scale set selection.** While SEL (the scale button, called SCL before SLOOP 2.5) is held, the PRESETS encoder cycles the four sets and KNOB2 scrolls within the current set.
- **Per-step cent bend.** A lockable track parameter CENT (−63..+63 cents) on the SEL 2 page. A step's bend is a CENT parameter lock, made with SLOOP 2.6's own lock gesture.
- **Display.** Note names on screen show half-flat and half-sharp glyphs.
- **Web editor.**
  - Note names parse and print quarter tones (`Ed4`, `E+4`).
  - The step SysEx messages carry cents (protocol version bump).
  - The scale list grows to 52 names.
- **Out of scope:**
  - microtonal MIDI out (notes are sent at their semitone, without the quarter tone) and microtonal MIDI in;
  - historical tuning profiles, Scala `.scl` import and export, more scale sets, and swapping the upper jins live (all recorded as future work in the design);
  - pytheory, Maqamusic, makam_studio and DiArMaqAr as dependencies or data sources: they served as research and cross-checks only, and the values are typed from the primary sources they cite.

## Capabilities

### New Capabilities
- `quarter-tone-pitch`: How notes are represented and sounded on a 24-tone grid. Covers the quarter-tone flag, the root in quarter tones, cent offsets reaching the oscillator, the 12-TET compatibility guarantee, and MIDI out rounding.
- `scale-sets`: The table of 52 scales in four sets: 24-tone ascending/descending masks, per-degree intonation and the EQUAL / TRADITIONAL switch, any scale on any root, scale files as extensible data, append-only indices, names, set selection in the SCL layer, and the list exposed to the web editor.
- `microtonal-key-input`: How the 27 physical keys produce quarter tones. Covers SNAP and WHITE quantizing on the 24 grid, choosing ascending or descending by direction, the OCT− / OCT+ half-flat / half-sharp modifier, and chords built from 24-tone scales.
- `step-cent-bend`: Microtonal sequencer steps. Covers a per-step cent offset, quarter tones kept through step editing and transposition, the project format and its migration, and step exchange with the web editor.

### Modified Capabilities
<!-- none: the project has no specs yet -->

## Impact

- **Firmware:**
  - Pitch path: `core.h` (`step_t`, `dstep_t`, `voice_t`), `voice.c` (`trk_note_on`, `voice_start`, block pitch).
  - Scales and keys: `seq.c` (`SCALE_MASK` → `SCALES`, `kb_map`, `chord_notes`, MIDI out), `params.c` (`N_SCALE`, `N_NOTE`, `P_ROOT`).
  - Persistence: `project.c` (a tagged "MQ" extension block inside SLOOP 2.6's unchanged FUN6 format).
  - UI: `ui_input.c` (octave buttons, SCL encoders, step knobs, key lights), `ui_layers.c` (SCL layer and screen, transpose), `ui_draw.c` / `ui.c` (note names), `gfx.c` (two glyphs).
  - Other: `upreset.c` (keeps Q), `editor.c` (step fields, DESC).
- **Build:** new scale files `scales/{west,turk,arab,india}.txt` and a new `tools/gen_scales.py` (standard library only) that emits `build/gen/scales.h`. `tools/build.py` runs it.
- **Settings:** one new global setting (intonation), saved in the project's extension block; projects without one load as TRADITIONAL.
- **Memory:** no change in project size (SLOOP 2.6's 3816 bytes; the extension uses fields upstream's loader skips), about 2 KB of flash for the scale table, and no new RAM tables.
- **Web:**
  - `web/editor.html` and its copy `docs/webapp/editor/index.html`.
  - `web/EDITOR_PROTOCOL.md` (protocol 11: the step's quarter-tone bits).
  - `web/test_web.mjs`.
- **Tests:** `tests/scale_test.c`, `project_test.c`, `seq2_test.c`, `regress.c` / `golden.txt` (the Western hashes must not change).
- **Docs:** `SLOOP.md` and `README.md` (the scale sets, quarter-tone input, cents).
- **Compatibility:**
  - Projects from SLOOP 2.6 and older load unchanged, through upstream's importers.
  - SLOOP 2.6 firmware can open this fork's projects, in 12-tone.
  - The MIDI out note numbers are unchanged.
