# Tasks

## 1. Host app (native)

- [x] 1.1 Create `web/emu/hal_host.h`:
  - every `fm1_*` symbol the firmware uses (starting from the `ui_pages_test.c` and `hostsim.c` stubs);
  - the `fm1_in` globals;
  - an LCD framebuffer;
  - LED masks;
  - simulated 1 MiB NOR flash with `st_read` / `st_erase` / `st_prog` and `fm1_xip_ptr` (design D3).

  Verify that a native compile of a file including it with the firmware sources links with no undefined symbols.
- [x] 1.2 Create `web/emu/sloop_host.c`: the firmware sources in `felucca.c` order (without `main.c`, `audio.c`, `lcd.c`, OTA, recovery and console), `host_boot()`, and `host_run_ms()` (design D2). Verify with a native build in `tests/run_tests.sh`.
- [ ] 1.3 If code in `main.c` / `audio.c` has to be shared rather than copied, move it into a header used by both. Verify that `build/felucca.fwsc` is byte-identical before and after (compare `sha256` of the outputs).
- [x] 1.4 Add `tests/hostapp_test.c` and wire it into `tests/run_tests.sh`. It must:
  - boot from blank flash;
  - press PLAY, render 4 bars, and match a new golden hash;
  - save, reboot from the same flash and check that the project is identical;
  - send a USB note on channel 1 and a TRS note with running status, and check that both voices start;
  - send clock with SYNC = USB and check the tempo;
  - hold OCT− + OCT+ for 5 s and check that the UBOOT path is not taken.

  Verify that `tests/run_tests.sh` passes.

## 2. WASM build

- [x] 2.1 Create `web/emu/sloop_web.c` with the exports from design D6: render, the input masks, encoders, MASTER, MIDI in (USB packet and TRS bytes), the MIDI out and SysEx rings, the framebuffer, LED masks, the flash pointer with dirty-sector bits, and the version. Verify that it builds natively next to `sloop_host.c`.
- [x] 2.2 Create `web/emu/build.sh`. It runs clang `--target=wasm32` with `wasm-ld` and the flags from D4, reuses `build/gen`, and outputs `build/emu/sloop.wasm` with no imports. Verify that the script produces the wasm on macOS and prints its size, and that the module's import list is empty.
- [x] 2.3 Add a headless Node check to `web/test_web.mjs`. It instantiates `build/emu/sloop.wasm` with no imports, boots, renders the hostapp golden scenario, and compares the hash with the native one. Verify that `node web/test_web.mjs` passes (and is skipped with a notice when the wasm hasn't been built).
- [x] 2.4 Credit fm1-x0x (Charles Vestal, GPL-3.0-only) in `LICENSING.md` for the emulator approach, the worklet and the SVG panel code. Verify that `LICENSING.md` lists the source URL and licence.

## 3. Emulator page

- [x] 3.1 Create `web/emu/index.html` with:
  - the SVG panel adapted from fm1-x0x, using SLOOP's labels (SCL), and invisible hit targets;
  - pointer, multi-touch and wheel input, with Shift-click / right-click latch;
  - the computer-keyboard map from the spec, and help text.

  Verify manually in Chrome, Firefox and Safari: a three-finger chord on touch (iPad or the device emulator in devtools), latched SCL plus a key, and `Q` holding F3.
- [x] 3.2 Inline the worklet (`<script type="text/worklet">` → Blob URL, D5), and add the Start control and error reporting. Verify that audio plays after Start, and that a renamed `sloop.wasm` shows a 404 error naming the file.
- [x] 3.3 Draw the screen (RGB565 → canvas at 30 Hz when changed), the button and key LEDs, the version, and the load %. Verify that the About screen version matches the page and that LIGHTS = HIGH lights every button.
- [x] 3.4 Persist to IndexedDB: dirty sectors are saved about once a second and restored before boot, with a reset that asks for confirmation and an "in memory only" notice if storage fails. Verify:
  - save a project, reload, and it is the same;
  - after a reset the emulator boots to factory settings;
  - in a private window the notice is shown.

## 4. Web MIDI in

- [x] 4.1 Add the MIDI input selector: Web MIDI access is requested lazily without SysEx, the device list updates live, and the device and route (USB/TRS) are remembered. Verify: picking a device plays notes; after a reload it is re-selected; in a browser without Web MIDI the selector is disabled with the spec text.
- [x] 4.2 USB route: pack messages into USB-MIDI packets and send them to `midi_in_event`. TRS route: send raw bytes to `um_byte`. Queue them in order before each render. Verify that a hardware keyboard plays the channel 1-3 parts, that channel 10 plays the drums, and that TRS running status from a sequencer plays every note.
- [x] 4.3 Clock: verify with a DAW at 100 BPM. With SYNC = USB and the USB route, start/stop/continue are followed. With SYNC = TRS and the USB route, the clock is ignored.
- [x] 4.4 Disconnect handling: send note-offs for held notes and show "disconnected". Verify by unplugging the keyboard while holding a key: no note hangs.

## 5. Modalities panel

- [x] 5.1 Add the `MODALITIES` registry and the generated panel with the eleven entries and their states (D8). Verify that the panel matches the spec list.
- [x] 5.2 MIDI out log: decoded entries with a timestamp and channel, keeping the last 200. Verify that a key press and release with MIDI out active shows a note-on and a note-off.
- [x] 5.3 Editor SysEx log: ignored input SysEx with its length and first bytes. Verify with a test injection of `F0 22 24 35 7D F7` through the adapter: it is logged and the emulator keeps running.
- [x] 5.4 Unavailable stubs: battery shows "no reading", the UBOOT combo shows the countdown and then a page toast, and samples, backup, update and console each give their reason. Verify by opening every screen that shows the battery, and by holding OCT− + OCT+ for 5 s.
- [x] 5.5 Check that the adapters can be swapped: replace `midiOut` with a no-op adapter in a dev build and confirm that nothing else changes. Verify with a code review checklist item in the PR description.

## 6. Publishing and docs

- [x] 6.1 Have `web/make_site.py` copy `build/emu/{index.html,sloop.wasm}` to `docs/webapp/emu/`. Verify that `python3 web/make_site.py` produces the folder and that the page works when served locally with `python3 -m http.server`.
- [x] 6.2 Add a "Try it in your browser" link to the README and a short emulator section to `SLOOP.md`: controls, MIDI in, what is stubbed, where data is kept. Verify that the links resolve on the local site build.

## 7. Integration checks

- [ ] 7.1 Run a full `./build.sh`, `tests/run_tests.sh`, `web/emu/build.sh` and `node web/test_web.mjs`. Verify that everything passes, that `build/felucca.fwsc` is unchanged from before the change, and that the native and WASM golden hashes are equal.
- [x] 7.2 A ten-minute play session in Chrome with a MIDI keyboard and DAW clock: record a pattern, save, reload, and replay. Verify with a written session log in the change folder: no stuck notes, no screen artefacts, and the data survives the reload.
