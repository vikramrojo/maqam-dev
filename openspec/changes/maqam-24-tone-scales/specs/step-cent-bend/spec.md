# Spec Delta

## Purpose

Lets sequencer steps carry quarter tones and a fine cent bend, so programmed patterns can follow real makam, maqam and raga intonation, and keeps those steps intact through editing, saving, migration and the web editor.

## ADDED Requirements

### Requirement: Per-step cent bend
Each synth step SHALL store a signed cent bend from −63 to +63, which applies to every note of the step. When the step plays, its notes SHALL sound with the bend added to the scale's per-degree intonation.

#### Scenario: Bent step
- **WHEN** a step with A4 and a bend of −12 plays in a WEST scale
- **THEN** A4 sounds 12 ± 1 cents flat

#### Scenario: Chord step
- **WHEN** a step with three notes and a bend of +8 plays
- **THEN** all three notes sound 8 cents sharp, plus each note's own scale intonation

### Requirement: Editing the bend on the step page
The step page SHALL provide a CENT control that sets the selected step's bend from −63 to +63, one cent per detent. A step's tile SHALL show its bend when it is non-zero.

#### Scenario: Set a bend
- **WHEN** a step is held and the CENT knob is turned 5 detents counter-clockwise
- **THEN** the step's bend becomes −5, and its tile shows −5c

### Requirement: Quarter tones survive step editing
Turning the NOTE control SHALL move a step's note by semitones and keep its quarter tone. Holding OCT− / OCT+ while turning NOTE SHALL move it by quarter tones. Pattern transposition SHALL keep each note's quarter tone.

#### Scenario: Transpose a half-flat
- **WHEN** a pattern with E-half-flat is transposed up 2 semitones
- **THEN** that note becomes F#-half-flat

#### Scenario: Quarter-tone nudge
- **WHEN** a step holding E is held, OCT− is held and NOTE is turned one detent counter-clockwise
- **THEN** the step holds E-half-flat

### Requirement: Projects store quarter tones and bends
Saved projects SHALL keep every step's quarter-tone flags and bend. A project SHALL still fit the existing project storage and in-RAM project slots.

#### Scenario: Round trip
- **WHEN** a project with E-half-flat steps and bends of −63, 0 and +63 is saved and loaded
- **THEN** every note, flag and bend is identical

### Requirement: Older projects migrate
Projects saved before this change SHALL load with every step's bend set to 0, the root converted to the same pitch, and the scale unchanged. The music SHALL sound as before.

#### Scenario: Load a project from SLOOP 2.3
- **WHEN** a project saved by SLOOP 2.3 with root D and scale MIN is loaded
- **THEN** the root is D, the scale is MIN, every bend is 0, and the steps play the same notes

### Requirement: Damaged values are clamped on load
On load, bends outside −63..+63 SHALL be clamped into range. Note bytes with the quarter-tone flag SHALL be accepted.

#### Scenario: Out-of-range bend
- **WHEN** a project holding a bend of −100 is loaded
- **THEN** the bend becomes −63

### Requirement: Web editor exchanges microtonal steps
The web editor's step messages SHALL carry each step's bend and the quarter-tone flags of its notes, under a new protocol version. The editor SHALL print and accept quarter-tone note names, with `d` for half-flat and `+` for half-sharp (e.g. `Ed4`, `F+3`).

#### Scenario: Read a microtonal step
- **WHEN** the editor reads a step holding E-half-flat 4 with a bend of +7
- **THEN** it shows `Ed4` with +7 cents

#### Scenario: Write a microtonal step
- **WHEN** the user enters `Ed4` with −3 cents in the editor and sends it
- **THEN** the FM-1 step holds E-half-flat 4 with a bend of −3

### Requirement: User presets keep quarter tones
The pattern stored with a user preset SHALL keep its notes' quarter-tone flags. The preset format version SHALL not change.

#### Scenario: Preset pattern
- **WHEN** a user preset is saved with a pattern containing B-half-flat and loaded again
- **THEN** the pattern contains B-half-flat
