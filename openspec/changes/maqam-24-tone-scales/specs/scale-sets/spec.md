# Spec Delta

## Purpose

Defines the 52 scales SLOOP offers in four sets: Western, Turkish makam, Arabic maqam and Indian raga. Covers their 24-tone ascending/descending forms and intonation, how they keep their place in saved projects, and how the player and the web editor choose them.

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

### Requirement: Per-degree intonation
Each scale SHALL carry a cent offset for each of its quarter-tone positions. The offset is added to every note that falls on that position.

- WEST and ARAB offsets SHALL all be zero.
- TURK offsets SHALL place degrees within ±3 cents of the 53-comma tuning.
- INDIA offsets SHALL place degrees within ±3 cents of the just-intonation swaras.

#### Scenario: Turkish Rast third
- **WHEN** the third degree of TURK Rast is played on C
- **THEN** it sounds 385 ± 3 cents above C

#### Scenario: Same notes, different tuning
- **WHEN** TURK Segah and TURK Kürdi are played on the same root and the same degree
- **THEN** the notes are the same, but the pitches differ by the scales' intonation

#### Scenario: Indian Bhupali major third
- **WHEN** the Ga of INDIA Bhupali is played
- **THEN** it sounds 386 ± 3 cents above Sa

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
