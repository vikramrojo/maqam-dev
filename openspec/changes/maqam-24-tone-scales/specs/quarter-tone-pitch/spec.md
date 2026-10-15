# Spec Delta

## Purpose

Defines how SLOOP represents and sounds pitch on a 24-tone (quarter-tone) grid with cent offsets, while keeping every 12-tone note and sound exactly as before.

## ADDED Requirements

### Requirement: Notes carry an optional quarter tone
A synth note SHALL be a semitone (MIDI 0..127) plus an optional quarter-tone flag that raises it by 50 cents. The note's 24-tone position SHALL be `semitone × 2 + flag`.

#### Scenario: Half-flat E
- **WHEN** a synth note is E-half-flat in octave 4
- **THEN** it is stored as D#4 with the quarter-tone flag, and sounds 50 cents below E4

#### Scenario: Plain note unchanged
- **WHEN** a synth note has no quarter-tone flag
- **THEN** its stored value equals its MIDI note number, as before this change

### Requirement: Quarter tones sound at the correct pitch
A voice SHALL sound a flagged note exactly 50 cents above its semitone, within ±1 cent, on every synth engine. This SHALL include mono, legato, unison and glide modes.

#### Scenario: Quarter tone rendered
- **WHEN** C4 with the quarter-tone flag is played on any synth engine
- **THEN** the measured fundamental is 50 ± 1 cents above C4

#### Scenario: Glide between quarter tones
- **WHEN** glide is on and E-half-flat follows D
- **THEN** the pitch glides to and settles at E-half-flat, not E or E-flat

### Requirement: Cent offsets reach the oscillator
A note-on SHALL accept a signed cent offset, which shifts the voice pitch by that many cents. The total offset SHALL be clamped to ±100 cents. It SHALL be applied on top of the global TUNE, transpose, the pitch LFO, the pitch envelope and unison detune.

#### Scenario: Positive offset
- **WHEN** A4 is played with an offset of +20 cents
- **THEN** it sounds 20 ± 1 cents above 440 Hz (with TUNE at 0)

#### Scenario: Offset clamp
- **WHEN** the step bend and the scale tuning add up to more than +100 cents
- **THEN** the note sounds +100 cents above its grid pitch

### Requirement: 12-tone material is bit-identical
Notes without a quarter-tone flag or cent offset, played in a WEST scale, SHALL render audio identical to the firmware before this change.

#### Scenario: Golden renders
- **WHEN** the existing regression renders run, which contain only 12-tone material
- **THEN** their audio hashes match the existing golden values

### Requirement: Scale root in quarter tones
The scale root SHALL take 24 values, one per quarter tone from C. The even values are the 12 semitone roots.

#### Scenario: Root on E-half-flat
- **WHEN** the root is set to E-half-flat with a scale selected
- **THEN** the scale's first degree sounds at E-half-flat

### Requirement: MIDI out sends the semitone
Notes sent over MIDI SHALL use the note's semitone, without the quarter tone. Outgoing note numbers SHALL stay those of the semitone.

#### Scenario: Half-flat sent out
- **WHEN** E-half-flat (stored as D# plus the flag) is played on a key with MIDI out active
- **THEN** a note-on for D# is sent, and the matching note-off releases D#

### Requirement: MIDI in stays 12-tone
Notes received over MIDI SHALL be played as 12-tone semitones, without quarter-tone flags or cent offsets, as before this change.

#### Scenario: Incoming note
- **WHEN** MIDI note 64 arrives on a synth part's channel
- **THEN** the part plays E4 with no quarter tone and no cent offset
