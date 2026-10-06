# Spec Delta

## Purpose

Shows the emulator's visitors what each FM-1 input and output feature does in the browser (working, logged, covered or not available), with a defined stub for each, so later work can make a stub real without redesigning the page.

## ADDED Requirements

### Requirement: Modalities panel
The page SHALL have a panel listing every FM-1 modality SLOOP uses. Each SHALL show one state from: **working**, **logged**, **covered** or **not available**. Each entry SHALL say what happens in the emulator in one line. The list SHALL be:

- **Working:** MIDI in (USB), MIDI in (TRS), MIDI clock in.
- **Logged:** MIDI out, web editor SysEx.
- **Covered:** USB audio, by the page's own output.
- **Not available:** sample upload and CHOP, backup and restore, firmware update and UBOOT, battery level, serial console.

#### Scenario: Panel contents
- **WHEN** the emulator has booted
- **THEN** the panel lists all eleven modalities with their state and description

### Requirement: MIDI out is logged
Every MIDI message the firmware sends SHALL be shown in a MIDI out log on the page, with a timestamp, the channel and a decoded message type. The log SHALL keep at least the last 200 messages. Messages SHALL NOT be sent to any Web MIDI output.

#### Scenario: Key press echoes
- **WHEN** a synth key is pressed and released with MIDI out active
- **THEN** the log shows a note-on and a note-off with the firmware's channel and note

### Requirement: Web editor SysEx is logged
SysEx messages that the firmware would send or receive SHALL be counted and shown in the editor log, with their length and the first bytes. The editor connection itself is not emulated.

#### Scenario: Ignored incoming SysEx
- **WHEN** the MIDI input delivers a SysEx message
- **THEN** the editor log shows it as ignored, with its length

### Requirement: Unavailable modalities are explicit
The firmware SHALL see each unavailable modality the way the device sees it when that modality is absent or idle:

- no USB host for update or SysEx commands;
- a battery reading of "unknown";
- no serial console.

The page SHALL show each as not available with its reason. No unavailable modality SHALL crash or stall the emulator.

#### Scenario: Battery
- **WHEN** a screen that shows the battery is opened
- **THEN** it shows the device's "no reading" state, and the emulator keeps running

#### Scenario: UBOOT combination
- **WHEN** OCT− and OCT+ are held for 5 seconds while stopped
- **THEN** the emulator does not enter update mode, and the page notes that UBOOT is not available in the emulator

### Requirement: Stubs are swappable
Each modality SHALL connect through a single page-side adapter with a defined message interface. A later change can then replace a stub with a real version (Web MIDI out, editor loopback, sample upload) without changing the firmware build or the other adapters.

#### Scenario: Replace MIDI out stub
- **WHEN** a developer replaces the MIDI out adapter with a Web MIDI output adapter
- **THEN** no other adapter and no WASM export has to change
