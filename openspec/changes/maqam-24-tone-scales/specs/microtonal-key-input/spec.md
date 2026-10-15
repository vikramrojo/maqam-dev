# Spec Delta

## Purpose

Defines how the FM-1's 27 chromatic keys produce quarter tones, using scale quantizing, the direction of play and a half-flat / half-sharp modifier, without changing the keyboard layout.

## ADDED Requirements

### Requirement: SNAP plays the key's note, else follows the direction
In SNAP mode, each key SHALL cover its own semitone and a quarter tone either side.
- If a degree of the current form lies within that window, the key SHALL play it. The exact semitone is preferred; if both quarter tones are degrees, the one in the direction of travel is used.
- If no degree lies within the window, a key played going up SHALL play the next degree above. A key played going down SHALL play the next degree below.

This applies to every scale, Western ones included.

#### Scenario: E key in Arabic Rast
- **WHEN** SNAP is on, the scale is ARAB Rast on C, and the E key is pressed
- **THEN** E-half-flat plays

#### Scenario: E-flat key in Arabic Rast
- **WHEN** SNAP is on, the scale is ARAB Rast on C, and the E-flat key is pressed
- **THEN** E-half-flat plays

#### Scenario: A gap going up
- **WHEN** SNAP is on, the scale is INDIA Asavari on C, D was the previous key, and the E key is pressed (going up)
- **THEN** F plays: Asavari has no 3rd going up, so the key moves on to the next note

#### Scenario: The same key going down
- **WHEN** SNAP is on, the scale is INDIA Asavari on C, F was the previous key, and the E key is pressed (going down)
- **THEN** E-flat plays: the descending form's 3rd

#### Scenario: Western SNAP follows the direction
- **WHEN** SNAP is on, the scale is WEST MAJ on C, and the C# key is pressed
- **THEN** D plays going up, and C plays going down

### Requirement: WHITE walks 24-tone scale degrees
In WHITE mode, successive white keys SHALL play successive degrees of the scale, starting with the root at C4. Black keys SHALL stay silent.
- **Layout:** the layout SHALL come from the form with more degrees, so a key keeps its place when the direction changes.
- **Missing degrees:** a degree missing from the form being played SHALL be replaced by the next degree in the direction of travel: above going up, below going down.

#### Scenario: Seven-degree maqam
- **WHEN** WHITE is on, the scale is ARAB Bayati on D, and the white keys C4..C5 are pressed in turn
- **THEN** the degrees D, E-half-flat, F, G, A, B-flat, C, D play in order

#### Scenario: Asavari up and down
- **WHEN** WHITE is on, the scale is INDIA Asavari on C, and the white keys C4..C5 are played up and then down
- **THEN** going up plays C D F F G A-flat C C, and going down plays C D E-flat F G A-flat B-flat C

### Requirement: Direction chooses ascending or descending
On a synth track, a key lower than the previous key pressed on that track SHALL use the scale's descending form, and any other key the ascending form. After a pause of more than 1 second since that track's previous key, the next key SHALL count as ascending. The note that plays SHALL be the note that is recorded.

#### Scenario: Rast up then down
- **WHEN** WHITE is on, the scale is ARAB Rast on C, and the player goes up to the seventh degree and then comes back down to it
- **THEN** B-half-flat plays on the way up and B-flat on the way down

#### Scenario: A new phrase after a pause
- **WHEN** the previous key was G4, more than 1 second has passed, and E4 is pressed
- **THEN** E4 uses the ascending form

#### Scenario: Recorded as played
- **WHEN** a descending B-flat is recorded in REC
- **THEN** the step holds B-flat and replays B-flat, whichever direction the playback is in

### Requirement: The direction is shown for two-form scales
While SEL is held, the subtitle SHALL show an up or down arrow for the current direction when the selected scale's ascending and descending forms differ. It SHALL show no arrow for other scales.

#### Scenario: Asavari after a descending key
- **WHEN** INDIA Asavari is selected, the last key was played going down, and SEL is held
- **THEN** the subtitle shows a down arrow

#### Scenario: Major has no arrow
- **WHEN** WEST MAJ is selected and SEL is held
- **THEN** the subtitle shows no arrow

### Requirement: Half-flat and half-sharp modifier
On a synth track, holding OCT− while pressing a key SHALL lower the key's note by one quarter tone, and holding OCT+ SHALL raise it by one. Pressing OCT− / OCT+ while keys are held SHALL play those keys again (a new attack) a quarter tone lower / higher, and letting the button go SHALL play them again at the key's own note. While recording, the shifted note SHALL be recorded as played. This modifier SHALL also apply when choosing the root in the SEL layer. On the drum track, OCT− / OCT+ SHALL keep their ghost and hard-hit behavior.

#### Scenario: Chromatic half-flat
- **WHEN** the scale is CHR, OCT− is held and the E key is pressed
- **THEN** E-half-flat plays

#### Scenario: A held key shifted
- **WHEN** the scale is CHR, the E key is held, and then OCT− is pressed
- **THEN** E stops and E-half-flat plays; when OCT− is let go, E-half-flat stops and E plays again

#### Scenario: Root half-flat
- **WHEN** SEL and OCT− are held and the E key is pressed
- **THEN** the root becomes E-half-flat

### Requirement: Octave shift by double tap
On a synth track, a double tap of OCT− / OCT+ SHALL change the octave by one, on the second press. A tap counts only if it is short (at most 300 ms), no key is down and no knob is turned while it is held. The second press SHALL follow within 300 ms. A single tap SHALL not change the octave. Pressing both buttons SHALL still reset the octave to 0.

#### Scenario: Double tap
- **WHEN** OCT+ is tapped twice within 300 ms with no key down
- **THEN** the octave goes up by one on the second press

#### Scenario: Single tap
- **WHEN** OCT+ is tapped once
- **THEN** the octave is unchanged

#### Scenario: Modifier use keeps the octave
- **WHEN** a key is held and OCT+ is tapped twice
- **THEN** the key plays a quarter tone higher twice, and the octave is unchanged

### Requirement: Chords follow 24-tone scales
In chord mode, chords SHALL stack degrees of the current 24-tone scale. A POWER chord SHALL add the perfect fifth and the octave.

#### Scenario: Neutral triad
- **WHEN** chord mode is TRIAD, the scale is ARAB Rast on C, and the root degree is played
- **THEN** C, E-half-flat and G sound

### Requirement: Chord modes with exact intervals
Chord mode SHALL also offer 8VA (the note and its octave), QUART (three fourths of 500 cents) and DRONE (the note over the scale's root, the nearest one below it). On scales outside the WEST set, SUS4 SHALL use an exact 500-cent fourth and 700-cent fifth, and QUART SHALL use exact fourths. On WEST scales, SUS4 and QUART SHALL use the scale's own degrees. The CHORD+ modifiers other than the inversion SHALL not apply to POWER, 8VA, QUART, DRONE or the exact SUS4.

#### Scenario: Exact fourths in Saba
- **WHEN** chord mode is QUART, the scale is ARAB Saba on D, and D4 is played
- **THEN** D4, G4 and C5 sound, although Saba's own fourth above D is G-flat

#### Scenario: Drone
- **WHEN** chord mode is DRONE, the root is D, and A4 is played
- **THEN** D4 and A4 sound

#### Scenario: Western quartal
- **WHEN** chord mode is QUART, the scale is WEST MAJ on C, and F4 is played
- **THEN** F4, B4 and E5 sound

### Requirement: Key lights show quarter-tone scales
In the SEL layer, a key SHALL be lit as in the scale when its semitone, or a quarter tone either side of it, is a degree of the scale.

#### Scenario: E lit in Rast
- **WHEN** SEL is held with ARAB Rast on C
- **THEN** the E key is lit as in the scale
