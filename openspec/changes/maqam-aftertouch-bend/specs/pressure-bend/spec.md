# Spec Delta

## Purpose

Lets pressure on a held key bend its note continuously by a few cents, on the keys that can measure pressure, without changing anything upstream SLOOP reads or writes.

## ADDED Requirements

### Requirement: Pressure is opt-in at build time
Pressure sensing SHALL be compiled only with the `MAQAM_PRESS` build flag. Without it, the firmware SHALL contain none of the pressure code, globals, pages or stored bytes.

#### Scenario: Default build
- **WHEN** the firmware source is preprocessed without `MAQAM_PRESS`
- **THEN** it contains no pressure code (`fm1_press`, `press_block`, `press_q`, `G_AFT`, the sense hooks)

### Requirement: Pressure bends held notes
With AFT ON and a key among F3..C5 held, the key's notes SHALL bend up by pressure × 25 cents / 127, smoothed. The bend SHALL go back to 0 when the pressure does. Keys above C5 SHALL not bend. Each key SHALL bend on its own pressure, and a chord key SHALL bend all its notes.

#### Scenario: Full pressure
- **WHEN** AFT is ON and A4 is held at full pressure
- **THEN** A4 sounds 25 ± 2 cents sharp

#### Scenario: AFT off
- **WHEN** AFT is OFF
- **THEN** every render is identical to a build without pressure

### Requirement: Pressure bend is a performance gesture
Pressure bend SHALL not be recorded into steps and SHALL not change step data.

#### Scenario: Recording
- **WHEN** REC is on and a key is pressed harder while held
- **THEN** the recorded step holds the key's note without a bend

### Requirement: AFT is stored without changing upstream's format
AFT SHALL be stored in the MQ extension block, bytes 9 and 10 (the value and its complement), with the block's version and its byte-8 check unchanged. A project without a valid AFT field SHALL load with AFT OFF.

#### Scenario: Upstream project
- **WHEN** a project saved by SLOOP 2.6 is loaded
- **THEN** AFT is OFF

#### Scenario: Older fork build
- **WHEN** a project saved with AFT ON is loaded by a fork build from before this change
- **THEN** it loads normally, with its quarter tones, intonation and CENT

#### Scenario: Upstream firmware
- **WHEN** a project saved with AFT ON is loaded by SLOOP 2.6
- **THEN** it loads as a SLOOP 2.6 project

### Requirement: MIDI out sends poly pressure only with AFT on
With AFT on and MIDI out enabled, each held key's pressure SHALL be sent as poly key pressure on its track's channel, for the note's semitone. With AFT OFF, the MIDI output SHALL be unchanged.

#### Scenario: Pressure out
- **WHEN** AFT is ON and E-half-flat is held with rising pressure
- **THEN** poly key pressure messages for D# (63) are sent
