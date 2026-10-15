# Spec Delta

## Purpose

Runs the real SLOOP firmware in a single web page, with no FM-1 attached, so anyone can play it and every change can be checked by musicians and testers before it reaches hardware.

## ADDED Requirements

### Requirement: Same firmware sources
The emulator SHALL be built from the same firmware sources as the device, for the UI, sequencer, engines, FX, parameters, presets and storage format. Emulator code SHALL only replace hardware access. The DSP SHALL be compiled with the same floating-point contraction setting as the device.

#### Scenario: Golden render parity
- **WHEN** the emulator's host build renders a scenario from the regression suite
- **THEN** the audio hash equals the one in `tests/golden.txt`

#### Scenario: Device build unaffected
- **WHEN** `./build.sh` is run with this change applied
- **THEN** `build/felucca.fwsc` is byte-identical to the build before this change

### Requirement: Single page
The emulator SHALL be one HTML page that loads one WebAssembly module. The audio worklet code SHALL be inlined into the page. The emulator SHALL need no server code and no build step in the browser.

#### Scenario: Static hosting
- **WHEN** `index.html` and `sloop.wasm` are served from any static host, including GitHub Pages
- **THEN** the emulator starts after the user presses Start

### Requirement: Audio-driven clock
The audio callback SHALL advance the simulated device one millisecond at a time:

- The engine SHALL render in blocks of the device's I2S half-buffer size.
- The UI SHALL run at the device's frame interval.
- Output SHALL be 44.1 kHz stereo.

#### Scenario: Tempo accuracy
- **WHEN** the sequencer plays at 120 BPM for 60 seconds of audio
- **THEN** exactly 480 quarter-note steps have played

#### Scenario: Slow browser
- **WHEN** the browser cannot run the device in real time
- **THEN** audio drops out, but the firmware's time does not jump and no steps are skipped

### Requirement: Panel drawing and input
The page SHALL show an FM-1 panel with MASTER, SELECT, PRESETS, ALGORITHM, KNOB1-4, the 14 buttons and the 27 keys.

- Pointer, multi-touch and the mouse wheel SHALL operate them.
- Encoders SHALL turn in detents.
- Holding a button with Shift-click or right-click SHALL latch it, so it can be held while other controls are used.

#### Scenario: Chord on touch
- **WHEN** three keys are touched at once on a touch screen
- **THEN** the firmware sees all three keys held

#### Scenario: Hold a layer with a mouse
- **WHEN** SCL is Shift-clicked and then a key is clicked
- **THEN** the firmware sees SCL held while the key is pressed, as in the SCL layer

### Requirement: Computer keyboard
The page SHALL map computer keys to the panel:

- White keys 1-16 SHALL be `Q`-`I` and `A`-`K`.
- Black keys 1-11 SHALL be `1`-`0` and `-`.
- `Space` SHALL be PLAY, `Enter` REC, `Z` / `X` OCT− / OCT+, and `Esc` HOME.
- `↑` / `↓` SHALL turn SELECT.

The page SHALL show this mapping.

#### Scenario: Play from the keyboard
- **WHEN** `Q` is pressed and held
- **THEN** the first white key (F3) is held until `Q` is released

### Requirement: Screen and lights
The page SHALL show the firmware's 240×240 screen within 1/30 s of each change. It SHALL also light the button and key LEDs the firmware drives, including the LIGHTS / KEYS / NOTES settings.

#### Scenario: Screen update
- **WHEN** a key is pressed on the synth track
- **THEN** the screen shows the same change the device would within 1/30 s

### Requirement: Saved data in the browser
The emulator SHALL simulate the FM-1's flash data area and keep it in the browser across page reloads. That covers projects, the autosave, user presets, user samples and settings. The page SHALL offer a reset that erases the data, after a confirmation.

#### Scenario: Survive a reload
- **WHEN** a project is saved with SAVE and the page is reloaded
- **THEN** the project loads with the same steps and sounds

#### Scenario: Reset
- **WHEN** the user confirms the reset
- **THEN** the emulator boots as a new FM-1 with factory settings

### Requirement: Start requires a user gesture
Audio SHALL start only after the user presses a Start control. If a step fails (the WASM can't be fetched, there is no AudioWorklet, or audio is blocked), the page SHALL show what failed.

#### Scenario: WASM missing
- **WHEN** `sloop.wasm` returns HTTP 404
- **THEN** the page shows an error naming `sloop.wasm` and its status

### Requirement: Version shown
The page SHALL show the firmware version that the WASM module was built from.

#### Scenario: About
- **WHEN** the emulator has booted
- **THEN** the page shows the same version string as the device's About screen
