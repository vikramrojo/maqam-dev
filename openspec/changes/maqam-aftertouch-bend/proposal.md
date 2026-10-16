# Proposal

## Why

Maqam, makam and raga melodies bend into notes: a player leans on a note and pulls it a few cents sharp or flat. The fork's quarter tones and CENT locks fix pitches in advance; nothing lets a held note move continuously under the finger. The FM-1's keys are on/off domes, but fm1-velocity (Charles Vestal, GPL-3.0-only) shows that 20 of the 27 keys can be read as a resistance through the AC79's SARADC while held, giving a usable pressure value. Turning that pressure into a pitch bend would make the keyboard microtonally expressive.

The fork follows upstream SLOOP closely and merges its releases. This change must therefore be **forward compatible with SLOOP**: it must not change upstream's project, preset, settings or editor formats, must leave a default build behaving exactly like the current one, and must keep the HAL changes small and isolated so upstream merges stay easy.

## What Changes

- **Per-key pressure (HAL, opt-in).**
  - A build flag `MAQAM_PRESS` (default off until it passes on-device checks) enables a pressure reading for keys 0..19 (F3..C5), after fm1-velocity's DRIVE method: a held key's row is driven weakly every other scan and read on SARADC channels 0/1.
  - The reading is relative to the key's landing (its first ~5 ms), so per-unit calibration is not needed.
  - The result is `fm1_press[27]` (0..127; always 0 for keys 20..26). Note on/off stays on the existing digital scan.
  - The scan's ADC use moves the MASTER pot (and the battery reading) into scan slots when the flag is on.
- **Pressure bend.**
  - A new global **AFT**: OFF / ON (default OFF), on its own GLO page, **PRESS**.
  - When AFT is ON, each held key's pressure bends that key's voices **up**, by up to **25 cents**, smoothed (~25 ms), on top of quarter tones, scale intonation, CENT and MIDI pitch bend. That is the lowest range, so it acts as a vibrato under the finger rather than a bend towards the next note. Only one direction is possible (see design D5).
  - A chord key bends all its notes. On MONO / LEGATO tracks the sounding key bends.
  - The bend is a performance gesture. It is not recorded into steps (the sequencer has no bend lane).
- **MIDI out.** With AFT on and MIDI out enabled, poly key pressure (0xA0, the note's semitone) is sent for each held key. Pitch bend is not sent, because it would bend every note on the channel.
- **A separate test package.** CI builds a second firmware with the flag on, `maqam-press-firmware-<sha>`, whose About screen reads `MAQAM 2.6 AFT`. The default package and the site are unchanged.
- **Storage.** AFT is kept in the fork's MQ extension block (bytes 9..10: the value and its own check), so:
  - SLOOP 2.6 reads the project as its own and ignores the block, as today;
  - fork builds from before this change read MQ v1 and ignore bytes 9..11;
  - projects without the field load with AFT OFF.
- **Out of scope:** velocity from key strikes; pressure on keys 20..26; MPE; recording pressure.
