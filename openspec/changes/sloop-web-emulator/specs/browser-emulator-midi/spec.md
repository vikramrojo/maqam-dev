# Spec Delta

## Purpose

Lets hardware MIDI keyboards, controllers and DAW clocks drive the browser emulator through Web MIDI, entering the firmware through the same paths as on the device.

## ADDED Requirements

### Requirement: Choose a MIDI input
The page SHALL offer a MIDI input selector that lists the available Web MIDI inputs, and "None". It SHALL request MIDI access only when the user opens or uses the selector. The choice SHALL be remembered across reloads.

#### Scenario: Pick a keyboard
- **WHEN** the user picks "Keystep" in the selector
- **THEN** messages from that device reach the emulator

#### Scenario: Remembered choice
- **WHEN** the page is reloaded and the same device is present
- **THEN** it is selected again without the user choosing it

### Requirement: Route as USB or TRS MIDI
The page SHALL route the selected input in as either USB MIDI or TRS MIDI. USB is the default.

- Input routed as TRS SHALL go through the firmware's TRS MIDI parser byte by byte.
- Input routed as USB SHALL arrive as USB-MIDI event packets.

#### Scenario: TRS running status
- **WHEN** TRS routing is selected and the device sends notes using running status
- **THEN** every note plays, as through the FM-1's TRS jack

### Requirement: Notes follow the firmware's channel routing
Incoming notes SHALL be handled by the firmware's MIDI routing:

- channels 1-3 → synth parts;
- the drum channel → drums;
- any other channel → the selected track.

Note-offs and note-ons with velocity 0 SHALL release the note.

#### Scenario: Drum channel
- **WHEN** a note 36 on channel 10 arrives
- **THEN** the drum track plays its kick lane

### Requirement: MIDI clock in
Realtime messages (clock, start, continue, stop) SHALL reach the firmware with the source the routing names. When the firmware's SYNC setting matches that source, the sequencer SHALL follow the incoming clock.

#### Scenario: DAW sync
- **WHEN** SYNC = USB, USB routing is selected, and a DAW sends start and then clock at 100 BPM
- **THEN** the emulator plays at 100 BPM, and stops on stop

#### Scenario: Source mismatch
- **WHEN** SYNC = TRS and the input is routed as USB
- **THEN** the incoming clock is ignored, as on the device

### Requirement: Timing of MIDI input
The page SHALL deliver MIDI messages to the emulator in arrival order. Each message SHALL be applied no later than the next audio quantum after it arrives.

#### Scenario: Ordering
- **WHEN** note-on and note-off for the same key arrive within 1 ms
- **THEN** the firmware sees the note-on before the note-off

### Requirement: SysEx is not sent to the firmware
Incoming SysEx SHALL NOT be passed to the firmware. It SHALL be counted and shown in the modalities panel's editor log. This keeps update and UBOOT commands from acting on the emulator.

#### Scenario: UBOOT key
- **WHEN** a device sends `F0 22 24 35 7D F7`
- **THEN** the emulator keeps running and the log shows one ignored SysEx

### Requirement: Permission and disconnect handling
If Web MIDI is unavailable or permission is denied, the page SHALL say so next to the selector and keep working without MIDI. If the selected device disconnects, the page SHALL release any notes it was holding and show the device as disconnected.

#### Scenario: Unplugged mid-note
- **WHEN** a key is held on the MIDI keyboard and the keyboard is unplugged
- **THEN** the note is released and the selector shows "disconnected"

#### Scenario: No Web MIDI
- **WHEN** the browser has no `navigator.requestMIDIAccess`
- **THEN** the selector is disabled with the text "Web MIDI not supported in this browser"
