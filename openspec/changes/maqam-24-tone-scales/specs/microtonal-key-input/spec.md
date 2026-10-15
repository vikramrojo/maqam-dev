# Spec Delta

## Purpose

Defines how the FM-1's 27 chromatic keys produce quarter tones, using scale quantizing, the direction of play and a half-flat / half-sharp modifier, without changing the keyboard layout.

## ADDED Requirements

### Requirement: SNAP covers the quarter tone above each key
In SNAP mode, each key SHALL cover its own semitone and the quarter tone above it. The note SHALL be the highest scale degree at or below that quarter tone. For scales without quarter tones, the result SHALL equal SNAP before this change.

#### Scenario: E key in Arabic Rast
- **WHEN** SNAP is on, the scale is ARAB Rast on C, and the E key is pressed
- **THEN** E-half-flat plays

#### Scenario: E-flat key in Arabic Rast
- **WHEN** SNAP is on, the scale is ARAB Rast on C, and the E-flat key is pressed
- **THEN** E-half-flat plays

#### Scenario: Western SNAP unchanged
- **WHEN** SNAP is on, the scale is WEST MAJ on C, and the C# key is pressed
- **THEN** C plays, as before this change

### Requirement: WHITE walks 24-tone scale degrees
In WHITE mode, successive white keys SHALL play successive degrees of the scale, starting with the root at C4. Black keys SHALL stay silent.

#### Scenario: Seven-degree maqam
- **WHEN** WHITE is on, the scale is ARAB Bayati on D, and the white keys C4..C5 are pressed in turn
- **THEN** the degrees D, E-half-flat, F, G, A, B-flat, C, D play in order

### Requirement: Direction chooses ascending or descending
On a synth track, a key lower than the previous key pressed on that track SHALL use the scale's descending form. Any other key SHALL use the ascending form. The note that plays SHALL be the note that is recorded.

#### Scenario: Rast up then down
- **WHEN** WHITE is on, the scale is ARAB Rast on C, and the player goes up to the seventh degree and then comes back down to it
- **THEN** B-half-flat plays on the way up and B-flat on the way down

#### Scenario: Recorded as played
- **WHEN** a descending B-flat is recorded in REC
- **THEN** the step holds B-flat and replays B-flat, whichever direction the playback is in

### Requirement: Half-flat and half-sharp modifier
On a synth track, holding OCT− while pressing a key SHALL lower the key's note by one quarter tone, and holding OCT+ SHALL raise it by one. This modifier SHALL also apply when choosing the root in the SCL layer. On the drum track, OCT− / OCT+ SHALL keep their ghost and hard-hit behavior.

#### Scenario: Chromatic half-flat
- **WHEN** the scale is CHR, OCT− is held and the E key is pressed
- **THEN** E-half-flat plays

#### Scenario: Root half-flat
- **WHEN** SCL and OCT− are held and the E key is pressed
- **THEN** the root becomes E-half-flat

### Requirement: Octave shift on release
On a synth track, OCT− / OCT+ SHALL change the octave when released, and only if no key was pressed while the button was held. Pressing both buttons SHALL still reset the octave to 0.

#### Scenario: Plain octave change
- **WHEN** OCT+ is pressed and released with no key pressed in between
- **THEN** the octave goes up by one

#### Scenario: Modifier use keeps the octave
- **WHEN** OCT+ is held, a key is pressed, and OCT+ is released
- **THEN** the octave is unchanged

### Requirement: Chords follow 24-tone scales
In chord mode, chords SHALL stack degrees of the current 24-tone scale. A POWER chord SHALL add the perfect fifth and the octave.

#### Scenario: Neutral triad
- **WHEN** chord mode is TRIAD, the scale is ARAB Rast on C, and the root degree is played
- **THEN** C, E-half-flat and G sound

### Requirement: Key lights show quarter-tone scales
In the SCL layer, a key SHALL be lit as in the scale when its semitone, or a quarter tone either side of it, is a degree of the scale.

#### Scenario: E lit in Rast
- **WHEN** SCL is held with ARAB Rast on C
- **THEN** the E key is lit as in the scale
