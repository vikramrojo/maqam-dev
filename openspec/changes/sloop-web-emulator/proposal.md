# Proposal

## Why

Today SLOOP can only be played and checked on a physical FM-1. The coming 24-tone maqam change (`maqam-24-tone-scales`) has to be reviewed by musicians, most of whom don't own the device, and many of its acceptance checks are on-device steps.

A browser emulator that runs the real firmware can be ready before that work starts and serve as its review and QA tool. The fm1-x0x fork, another Felucca descendant, has already shown the approach works: it compiles the firmware's C sources to WebAssembly and runs them in an AudioWorklet behind an SVG drawing of the panel. Most of the pieces this needs already exist in SLOOP's host tests.

## What Changes

- **New host app.** A host build that runs SLOOP as a whole (UI, sequencer, engines, FX, storage and panel) against a simulated FM-1. It combines the HAL stubs that `tests/ui_pages_test.c` already uses with the DSP render loop of `tests/hostsim.c`.
- **WebAssembly build.** `web/emu/build.sh` compiles that host app into a standalone `sloop.wasm` with clang's `wasm32` target and `wasm-ld`. The firmware is freestanding, so no Emscripten and no JS glue are needed. It uses the same floating-point contract as the device.
- **A single emulator page, `web/emu/index.html`.**
  - It shows an SVG FM-1 panel adapted from fm1-x0x (GPL-3.0, credited): 7 encoders, the MASTER pot, 14 buttons and 27 keys.
  - It plays real-time stereo audio at 44.1 kHz from an inlined AudioWorklet.
  - It shows the 240×240 screen and the button and key lights.
  - Input works by pointer, touch, mouse wheel and computer keyboard.
- **Web MIDI in.** The page can pick a MIDI input device and route it in as USB MIDI or as TRS MIDI. Notes and MIDI clock (start, stop, continue, ticks) reach the firmware exactly as they do on the device, so a hardware keyboard can play the emulator and a DAW can clock it.
- **Saved data in the browser.** The FM-1's flash data area is simulated and kept in the browser's IndexedDB. Projects, user presets, settings and samples therefore survive a reload. A reset button erases them.
- **Stubs for the other device features.** A panel on the page lists each FM-1 feature with its state in the emulator:
  - **Working:** MIDI in (USB and TRS) and MIDI clock in.
  - **Shown in a log:** MIDI out and the web editor's SysEx traffic.
  - **Covered by the page's own sound:** USB audio.
  - **Shown as not available, with the reason:** sample upload and CHOP, backup and restore, firmware update and UBOOT, battery, and the serial console.

  Each stub is a clear place to plug in a real version later.
- **Docs and publishing.** A link from `README.md`. The page is built into `docs/webapp/emu/` next to the editor and installer.
- **Out of scope:**
  - Web MIDI out.
  - Live connection to the web editor.
  - Uploading samples from the page.
  - Emulating USB enumeration, UBOOT and firmware updates.
  - Matching device CPU timing: the overload guard is not fed by browser timing.

## Capabilities

### New Capabilities
- `browser-emulator`: Running the real SLOOP firmware in a browser page. Covers boot, the timing of audio and UI, the panel (controls, screen, lights), input methods, keeping saved data, and the requirement that the emulator uses the same sources as the device.
- `browser-emulator-midi`: Web MIDI input to the emulator. Covers choosing a device, routing it as USB or TRS MIDI, note and clock handling, and behavior when permission is denied or a device disconnects.
- `browser-emulator-modalities`: The page's panel of device features. Covers which are working, logged, covered or unavailable, and how each stub reports its state.

### Modified Capabilities
<!-- none: the project has no specs yet -->

## Impact

- **New files:**
  - `tests/hostapp.c` or `web/emu/sloop_host.c`, the combined host app (location set in the design).
  - `web/emu/sloop_web.c` (the WASM exports), `web/emu/index.html`, `web/emu/build.sh`.
- **Firmware:** no changes to how the device behaves. Small host-only guards (`#if FELUCCA_HOST` / `FELUCCA_WEB`) may be needed where `main.c` / `audio.c` logic must be reused rather than copied. The device build output (`build/felucca.fwsc`) stays byte-identical.
- **Build:** a new dependency on `wasm-ld` (LLVM lld), used only for the web emulator. On macOS it comes from `brew install lld`; Apple clang already provides the `wasm32` target. `./build.sh` and `tests/run_tests.sh` gain no new requirements.
- **Tests:**
  - A host smoke test of the combined app: boot, play, a golden render matching `regress.c`, a MIDI note and clock.
  - `web/test_web.mjs` extended with a headless WASM boot and render check under Node.
- **Publishing:**
  - `docs/webapp/emu/` (GitHub Pages).
  - `web/make_site.py` (or the equivalent copy step) gains the emulator.
- **Licensing:** the page's SVG and worklet code come from fm1-x0x (GPL-3.0-only). This is compatible, and they are credited in `LICENSING.md`.
- **Ordering:** this change lands before `maqam-24-tone-scales`, so its on-device checks and its musician review can use the emulator.
