# Spec Delta

## Purpose

Defines the 52 scales SLOOP offers in four sets: Western, Turkish makam, Arabic maqam and Indian raga, as an introduction to microtonal playing rather than a historical archive. Covers their 24-tone ascending/descending forms and intonation, the EQUAL / TRADITIONAL switch, playing any scale from any root, scale definitions as extensible data, how scales keep their place in saved projects, and how the player and the web editor choose them.

## ADDED Requirements

### Requirement: Four scale sets
SLOOP SHALL offer 52 scales in four sets:

- WEST: the 16 existing scales.
- TURK: 12 makams. Rast, Uşşak, Hüseyni, Kürdi, Nihavend, Hicaz, Hicazkar, Suzinak, Karcığar, Saba, Segah and Hüzzam.
- ARAB: 12 maqamat. Rast, Bayati, Saba, Sikah, Hijaz, Nahawand, Kurd, Ajam, Nikriz, Hijazkar, Suznak and Huzam.
- INDIA: 12 ragas. Yaman, Bilawal, Khamaj, Kafi, Asavari, Bhairavi, Bhairav, Todi, Purvi, Marwa, Bhupali and Malkauns.

#### Scenario: Count per set
- **WHEN** the scale list is enumerated
- **THEN** it holds 52 scales: 16 WEST, 12 TURK, 12 ARAB and 12 INDIA

### Requirement: Scale indices are stable
WEST scales SHALL keep indices 0..15 with their current meaning. TURK SHALL be 16..27, ARAB 28..39 and INDIA 40..51. Scales added later SHALL only be appended.

#### Scenario: Old project keeps its scale
- **WHEN** a project saved before this change with scale DOR is loaded
- **THEN** the scale is still DOR, with the same notes

### Requirement: Scales are defined on the 24-tone grid
Each scale SHALL define its degrees as quarter-tone positions above the root. WEST scales SHALL contain exactly their current 12-tone degrees.

#### Scenario: Arabic Rast degrees
- **WHEN** ARAB Rast is used on C, ascending
- **THEN** its degrees are C, D, E-half-flat, F, G, A, B-half-flat

#### Scenario: Western major unchanged
- **WHEN** WEST MAJ is used on C
- **THEN** its degrees are C, D, E, F, G, A, B, with no quarter tones

### Requirement: Ascending and descending forms
Each scale SHALL have an ascending and a descending form, which MAY differ.

#### Scenario: Arabic Rast descends with B-flat
- **WHEN** ARAB Rast on C is used descending
- **THEN** its seventh degree is B-flat instead of B-half-flat

#### Scenario: Yaman omits Pa ascending
- **WHEN** INDIA Yaman is used ascending
- **THEN** it has no Pa (the fifth)

#### Scenario: Yaman adds Pa descending
- **WHEN** INDIA Yaman is used descending
- **THEN** Pa is present

#### Scenario: Suznak descends through Rast's upper notes
- **WHEN** ARAB Suznak on C is used descending
- **THEN** its sixth and seventh degrees are A and B-flat instead of A-flat and B

#### Scenario: Huzam descends like Sikah
- **WHEN** ARAB Huzam on E-half-flat is used descending
- **THEN** its fourth and fifth degrees are A and B-flat instead of A-flat and B

### Requirement: Per-degree intonation
Each scale SHALL carry a cent offset for each of its quarter-tone positions, relative to its root. In TRADITIONAL intonation the offset is added to every note that falls on that position.

- WEST offsets SHALL all be zero.
- TURK offsets SHALL place degrees within ±3 cents of the Arel-Ezgi-Uzdilek Pythagorean ratios.
- ARAB offsets SHALL place degrees within ±3 cents of al-Ṣabbāgh's (1954) ratios.
- INDIA offsets SHALL place degrees within ±3 cents of the just-intonation swaras.
- Every offset SHALL be within ±25 cents: each pitch is rounded to the nearest quarter-tone step, so the offset is at most half a step.

#### Scenario: Turkish Rast third
- **WHEN** the third degree of TURK Rast is played on C
- **THEN** it sounds 385 ± 3 cents above C

#### Scenario: Same notes, different tuning
- **WHEN** TURK Segah and TURK Kürdi are played on the same root and the same degree
- **THEN** the notes are the same, but the pitches differ by the scales' intonation

#### Scenario: Indian Bhupali major third
- **WHEN** the Ga of INDIA Bhupali is played
- **THEN** it sounds 386 ± 3 cents above Sa

#### Scenario: Arabic Rast third
- **WHEN** the third degree of ARAB Rast is played on C in TRADITIONAL intonation
- **THEN** it sounds 357 ± 3 cents above C

### Requirement: Intonation switch
SLOOP SHALL offer a global intonation setting with two values, EQUAL and TRADITIONAL, saved with the project. The default SHALL be TRADITIONAL.

- In EQUAL, every scale SHALL sound on the plain 24-tone grid: no per-degree offsets are applied. Step cent bends still apply.
- In TRADITIONAL, the per-degree offsets SHALL apply.
- While SCL is held, turning the ALGORITHM encoder SHALL switch between the two, and the SCL screen SHALL show the current value.

#### Scenario: Hear the difference
- **WHEN** TURK Rast's third is held on C and the intonation is switched from TRADITIONAL to EQUAL
- **THEN** the pitch moves from 385 ± 3 cents to 400 ± 1 cents above C

#### Scenario: Western unaffected
- **WHEN** any WEST scale is played in EQUAL and in TRADITIONAL
- **THEN** the audio is identical

#### Scenario: Older projects
- **WHEN** a project saved before this change is loaded
- **THEN** the intonation is TRADITIONAL

### Requirement: Any scale on any root
Choosing a scale SHALL NOT change the root. Every scale SHALL keep its intervals and intonation on any of the 24 roots. For non-WEST scales, the SCL screen SHALL show the scale's traditional home tonic as a hint.

#### Scenario: Ajam on C
- **WHEN** the root is C and ARAB Ajam is selected
- **THEN** the root stays C, Ajam plays from C, and the screen shows the hint "home: B♭"

#### Scenario: Same intervals everywhere
- **WHEN** TURK Segah is played on D and then on F#-half-flat
- **THEN** the intervals between its degrees, including their cent offsets, are the same on both roots

### Requirement: Scales are extensible data
Scale definitions SHALL live in plain text files, one per set, that a build step turns into the firmware's scale table. Each pitch SHALL be written relative to the tonic, either in cents (with a decimal point) or as a ratio, as in Scala `.scl` files. A descending form MAY be given when it differs.

The build SHALL fail, naming the scale and the problem, if:
- a value lies exactly halfway between two quarter-tone steps, where the rounding would be ambiguous;
- two pitches of one scale fall on the same quarter-tone position with different offsets;
- a code is longer than 5 characters or is not unique;
- a shipped scale is missing, so its index would be lost.

#### Scenario: Add a scale
- **WHEN** a developer adds a new scale with cents or ratio values to any set's file and rebuilds
- **THEN** the scale gets the next free index in that set, with no firmware source changes, and no other scale moves

#### Scenario: Reordering lines keeps indices
- **WHEN** a developer moves an existing scale above another in its file and rebuilds
- **THEN** every shipped scale keeps its index

#### Scenario: Reject a removal
- **WHEN** a developer deletes a shipped scale from its file and rebuilds
- **THEN** the build fails and names the missing scale

### Requirement: Scale names fit the screens
Each scale SHALL have a code of up to 5 characters, unique across all 52 scales, for parameter pages and the web editor. It SHALL also have a name of up to 8 characters for the SCL screen. The Turkish Rast and Saba codes SHALL be distinguished from the Arabic ones.

#### Scenario: Unique codes
- **WHEN** all 52 codes are listed
- **THEN** none is longer than 5 characters, and no two are equal

### Requirement: Choosing a set and a scale in the SCL layer
While SCL is held:

- The PRESETS encoder SHALL cycle WEST, TURK, ARAB, INDIA and select the first scale of the chosen set.
- KNOB2 SHALL step through the scales of the current set only, wrapping at both ends.
- The SCL screen SHALL show the set and the scale name.

No new value SHALL be saved: the set follows from the scale.

#### Scenario: Next set
- **WHEN** SCL is held, WEST MAJ is selected and PRESETS is turned one step clockwise
- **THEN** TURK's first scale is selected

#### Scenario: Stay within the set
- **WHEN** SCL is held with the last ARAB scale selected and KNOB2 is turned clockwise
- **THEN** the first ARAB scale is selected

### Requirement: Web editor scale list
The web editor's parameter description for SCALE SHALL list all 52 scale codes in index order.

#### Scenario: DESC for scale
- **WHEN** the editor requests the description of SCALE
- **THEN** the reply lists 52 codes, and it fits in a single reply
