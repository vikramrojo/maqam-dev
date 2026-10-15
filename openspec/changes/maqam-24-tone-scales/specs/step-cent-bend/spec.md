# Spec Delta

## Purpose

Lets sequencer steps carry quarter tones and a fine cent bend, so programmed patterns can follow real makam, maqam and raga intonation. Keeps those steps intact through editing, saving, migration and the web editor, on top of SLOOP 2.6's parameter locks and project format.

## ADDED Requirements

### Requirement: CENT fine tune per track
Each synth track SHALL have a CENT parameter from −63 to +63 cents (default 0) on the SEL 2 page. Its notes SHALL sound with CENT added to the scale's per-degree intonation, clamped to ±100 cents in total.

#### Scenario: Track fine tune
- **WHEN** CENT is −12 on a track playing A4 in a WEST scale
- **THEN** A4 sounds 12 ± 1 cents flat

### Requirement: Per-step bend as a CENT lock
CENT SHALL be lockable, so a step's bend is a parameter lock of CENT on that step. It uses the same gesture, storage and display as other locks. A locked step's notes SHALL sound with the locked CENT, and the next step without a CENT lock SHALL return to the track's CENT.

#### Scenario: Bent step
- **WHEN** a step holding A4 has a CENT lock of −12 and the track's CENT is 0
- **THEN** that step's A4 sounds 12 ± 1 cents flat, and the next unlocked step is back in tune

#### Scenario: Chord step
- **WHEN** a step with three notes has a CENT lock of +8
- **THEN** all three notes sound 8 cents sharp, plus each note's own scale intonation

#### Scenario: Lock gesture
- **WHEN** SEQ and a step key are held, ALGORITHM selects CENT and PRESETS is turned 5 detents counter-clockwise
- **THEN** the step has a CENT lock of −5, marked as any locked step is

### Requirement: Quarter tones survive step editing
Turning the NOTE control SHALL move a step's note by semitones and keep its quarter tone. Holding OCT− / OCT+ while turning NOTE SHALL move it by quarter tones. Pattern transposition SHALL keep each note's quarter tone.

#### Scenario: Transpose a half-flat
- **WHEN** a pattern with E-half-flat is transposed up 2 semitones
- **THEN** that note becomes F#-half-flat

#### Scenario: Quarter-tone nudge
- **WHEN** a step holding E is held, OCT− is held and NOTE is turned one detent counter-clockwise
- **THEN** the step holds E-half-flat

### Requirement: Projects keep upstream's format with a fork extension
Projects SHALL be saved in SLOOP 2.6's own format (magic, layout and size unchanged).
- Quarter tones SHALL be kept in bit 7 of the note bytes.
- The fork's extra data SHALL be kept in a tagged extension block: the intonation setting, the quarter-tone half of each part's root, and each track's CENT.
- ROOT SHALL be stored in upstream's field as its semitone.
- CENT locks SHALL be stored as ordinary locks.

#### Scenario: Round trip
- **WHEN** a project with E-half-flat steps, a root of E-half-flat, EQUAL intonation, CENT values and CENT locks is saved and loaded
- **THEN** every note, flag, root, setting, CENT value and lock is identical

#### Scenario: Size unchanged
- **WHEN** the project structure is measured
- **THEN** it is exactly SLOOP 2.6's size, and the format magic is SLOOP 2.6's

### Requirement: Upstream projects and older formats load unchanged
A project without a valid extension block SHALL load with TRADITIONAL intonation, whole-semitone roots and CENT 0. Such a project is a SLOOP 2.6 project, an older format converted by upstream's importers, or one with a damaged block. Its music SHALL sound as on SLOOP 2.6.

#### Scenario: Load a SLOOP 2.6 project
- **WHEN** a project saved by SLOOP 2.6 with root D and scale MIN is loaded
- **THEN** the root is D, the scale is MIN, CENT is 0, intonation is TRADITIONAL, and it plays as on SLOOP 2.6

#### Scenario: Damaged extension
- **WHEN** the extension block's tag or check byte is wrong
- **THEN** the defaults are used, and the rest of the project loads normally

### Requirement: Fork projects degrade gracefully on upstream firmware
Upstream SLOOP 2.6 firmware loading a project saved by this fork SHALL see a valid SLOOP 2.6 project. Quarter tones fold to their semitones, the extension is ignored, and CENT locks are dropped.

#### Scenario: Open on upstream firmware
- **WHEN** a fork project with quarter tones is loaded by SLOOP 2.6 firmware
- **THEN** it loads without error and plays in 12-tone, at the stored semitone roots

### Requirement: Damaged values are clamped on load
On load, CENT values and CENT locks outside −63..+63 SHALL be clamped into range. Note bytes with the quarter-tone flag SHALL be accepted.

#### Scenario: Out-of-range CENT
- **WHEN** a project holding a CENT of −100 is loaded
- **THEN** CENT becomes −63

### Requirement: Web editor exchanges microtonal steps
The web editor's step messages SHALL carry the quarter-tone flags of a step's notes, under protocol 11 (upstream's 10 plus one byte). Bends SHALL travel as CENT locks through the existing lock messages. The editor SHALL print and accept quarter-tone note names, with `d` for half-flat and `+` for half-sharp (e.g. `Ed4`, `F+3`). With firmware older than protocol 11, the editor SHALL send and expect no quarter-tone byte.

#### Scenario: Read a microtonal step
- **WHEN** the editor reads a step holding E-half-flat 4 with a CENT lock of +7
- **THEN** it shows `Ed4`, and the step's lock list shows CENT +7

#### Scenario: Write a microtonal step
- **WHEN** the user enters `Ed4` in the editor and sends it
- **THEN** the FM-1 step holds E-half-flat 4

### Requirement: User presets keep quarter tones
The pattern stored with a user preset SHALL keep its notes' quarter-tone flags. The preset format version SHALL not change. Presets saved by SLOOP 2.6 SHALL load with CENT at 0, and presets saved by this fork SHALL load on SLOOP 2.6 with CENT ignored.

#### Scenario: Preset pattern
- **WHEN** a user preset is saved with a pattern containing B-half-flat and loaded again
- **THEN** the pattern contains B-half-flat
