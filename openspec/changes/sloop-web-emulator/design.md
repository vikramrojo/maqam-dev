# Design

## Context

See proposal.md (Why) and the three specs for requirements. The SLOOP code that shapes the approach:

- **Unity build.** `firmware/src/felucca.c` `#include`s every source after the header-only HAL (`fm1_*.h`). It also defines the flash backends `st_read` / `st_erase` / `st_prog` (`felucca.c:73…`).
- **Hardware access is direct, not abstracted.** The firmware calls about 25 `fm1_*` functions itself: `fm1_irq_on/off`, `fm1_ticks`, `fm1_wdt_feed`, `fm1_delay_ms`, `fm1_enc_take`, `fm1_input_edges`, `fm1_input_note_edges`, `fm1_adc_read`, `fm1_xip_ptr`, the LCD and LED calls, and the USB endpoint calls. It reads input from the global `fm1_in.{buttons,notes}`. Unlike fm1-x0x, SLOOP has no `plat.h` boundary.
- **The host tests already stub most of this, in two halves:**
  - `tests/ui_pages_test.c` compiles the whole UI (`ui*.c`, `gfx.c`, `panel.c`, `icons.c`, `splash.c`) against a `screen[240*240]` LCD and stubbed `fm1_*` input.
  - `tests/hostsim.c` compiles the DSP, sequencer, arranger, `usb.c` and `midi_uart.c` and calls `mix_block`.
  - Nothing yet runs both halves in one program.
- **Audio.** `audio.c` `audio_block()` = `mix_block()`, then the USB audio tap and a 24-bit shift. It is called from the I2S half-buffer ISR. The overload shedding lives in the ISR.
- **The main loop** (`main.c` `fm1_main`) does init (`persist_boot`, `settings_init`, `panel_init`, `felucca_init`, …) and then loops forever. Each pass reads the MASTER and battery ADCs, handles the UBOOT combo and the `ed_service` / `ota_service` SysEx, and runs the UI. It can't be called from a host because it never returns.
- **MIDI entry points:**
  - USB: `usb.c:623 midi_in_event(pkt)`.
  - TRS: `midi_uart.c:40 um_byte(b)`, the byte parser feeding `midi_in_q`.
  - The sequencer drains `midi_in_q` and sends clock through `mclk_event` with a source (1 = USB, 2 = TRS, taken from the cable nibble) (`seq.c:1672`).
  - MIDI out: `usb.c:126 midi_out_event(pkt)`.
- **Storage.** `storage.c` keeps A/B 4 KiB sectors with CRC in the 1 MiB SPI flash. User samples are read in place through `fm1_xip_ptr()` (`eng_sample.c:60`). The data area runs from 0x97000 to 0xFFFFF.
- **Web.** The static pages live in `docs/webapp/{editor,installer}` and are built by `web/make_site.py`. Node tests are in `web/test_web.mjs`.
- **Reference:** fm1-x0x `web/emu/`, with `build.sh` (emcc), `x0x_web.c` (exports), `worklet.js` and `index.html` (SVG panel, input, canvas, localStorage). It is GPL-3.0-only, the same licence as SLOOP.

## Goals / Non-Goals

**Goals:**
- Run the device's own source files with no changes to how the device behaves. The emulator only adds host code around them.
- One host app, built twice: natively with `cc` for tests, and as WASM for the page. Both builds use identical C.
- A page that keeps each modality behind a replaceable adapter.

**Non-Goals:**
- A `plat.h`-style refactor of the firmware (that could be a later change).
- Cycle-accurate CPU or overload behavior, USB enumeration, firmware update or UBOOT, the serial console.
- Web MIDI out, the editor loopback, sample upload from the page (all left as stubs).

## Decisions

### D1. A combined host app in `web/emu/`, shared with the tests
New `web/emu/sloop_host.c` (the "host app"):

- It includes `web/emu/hal_host.h`, which provides every `fm1_*` symbol the unity build uses.
- It then includes the firmware sources in `felucca.c`'s order, but **not** `main.c`, `audio.c`, `lcd.c`, the real HAL, or the parts that only work on the device (`ota.c`, `recovery.c`, `console.c`).
- `hal_host.h` grows from the stubs in `ui_pages_test.c` and `hostsim.c` (the `fm1_in` globals, edges, encoders, ticks, LEDs into masks, an LCD framebuffer).

`tests/hostapp_test.c` includes the same file for a native smoke test.

- **Why here, not under `tests/`:** the web build is a product; the tests consume it.
- **Alternative: X0X's `plat.h` refactor.** It is cleaner long term, but it touches nearly every firmware file and conflicts with the maqam change in flight.
- **Alternative: compile `felucca.c` with a fake HAL.** `main.c` and the ISRs can't be included without large `#ifdef`s.

### D2. Host-side equivalents of the main loop and the audio ISR
- **`host_boot()`** calls the init sequence from `fm1_main` in the same order, skipping hardware setup.
- **`host_run_ms(1)`:**
  - Increments `fm1_ms`.
  - Calls `mix_block` for every due 256-frame half-buffer and applies the same `OUT_SHIFT`. The steps after `mix_block` are copied from `audio.c`, with `FELUCCA_UAC 0` so there is no USB tap.
  - Runs the loop body (`ui_input`, `ui_leds`, `ui_draw` every 16 ms, MASTER from the page) and `ed_service` with no USB.
- **Cost:** a little duplicated logic, kept tiny on purpose. `hostapp_test` guards against drift by booting, playing a known project and checking against a golden render.
- **Small refactors are allowed** if a piece of `main.c` / `audio.c` is non-trivial, for example by moving the post-`mix_block` code into a `static inline` in a header used by both. Only refactors that leave `build/felucca.fwsc` byte-identical are allowed, and a task checks this.

### D3. Simulated flash backs storage and samples
- `hal_host.h` holds `static uint8_t flash[1 << 20]`, erased to 0xFF.
- `st_read` / `st_erase` / `st_prog` operate on it with NOR rules: programming can only clear bits.
- `fm1_xip_ptr(off)` returns `flash + off`.
- Projects, autosave, presets, settings and samples therefore all use their real code paths.
- **Persisting:** a write counter is exported. The worklet posts the dirty 4 KiB sectors of 0x97000-0xFFFFF to the page about once a second. The page stores them in IndexedDB (about 420 KiB at most) and hands them back before `host_boot()`.
- **Alternative: X0X's per-object `store[]` in localStorage.** SLOOP's storage is offset-based, not object-based, and the samples alone exceed the localStorage quota.

### D4. WASM build: clang + wasm-ld, freestanding (no Emscripten)
The firmware is freestanding: it includes only `<stdint.h>` and brings its own `libc.c`. So the web build needs no C runtime.

`web/emu/build.sh` therefore uses clang's built-in `wasm32` target and LLVM's `wasm-ld`:
```
clang --target=wasm32 -O2 -ffp-contract=off -nostdlib -fno-builtin -ffreestanding
      -DFELUCCA_HOST -DFELUCCA_WEB -Ibuild/gen -Ifirmware/src -Ifirmware/hal
      -Wl,--no-entry -Wl,--export-dynamic -Wl,-z,stack-size=1048576 -Wl,--initial-memory=...
      -o build/emu/sloop.wasm web/emu/sloop_web.c
```
- **Setup on macOS:** Apple clang already includes the `wasm32` target; `brew install lld` provides `wasm-ld`. Elsewhere, any LLVM ≥ 15 with lld works. No Docker and no emsdk are needed.
- **The module needs no imports**, so the page instantiates it with `{}`.
- **Generated headers:** it reuses the `build/gen` headers, which are produced by the generators in `tools/build.py` and need Pillow.
- **Output:** `build/emu/`. `web/make_site.py` copies it to `docs/webapp/emu/`.
- **Rejected: Emscripten (in Docker or from emsdk).** It is a heavier dependency, and its runtime is unnecessary for freestanding code. fm1-x0x needed it for `malloc` / `free` in its sample slots; SLOOP does not.

### D5. A single page with the worklet inlined
- `web/emu/index.html` holds the page, the SVG panel builder adapted from fm1-x0x (credited), and the worklet source in a `<script type="text/worklet">`. The page loads that source with `audioWorklet.addModule(URL.createObjectURL(new Blob(...)))`.
- `sloop.wasm` is the only other file. Inlining it as base64 was rejected: it adds about 33 %, slows parsing and can't be cached.
- **Changes from X0X's panel:**
  - The second button is labelled SCL, as SLOOP names it.
  - The latch hint names SLOOP's layers.
  - The help text lists SLOOP's controls.

### D6. WASM interface and messages between the page and the worklet
**Exports from `web/emu/sloop_web.c`** (all `uint32_t` / pointers unless noted). This is the contract between the host app and the page:

| Export | Meaning |
|---|---|
| `web_boot()` | boot from the current flash image (call after restoring the flash) |
| `web_render(n)` | run the device until n ≤ 1024 frames exist, into `web_out_l()` / `web_out_r()` (`float*`) |
| `web_buttons(mask)` | held buttons: **logical** bit `B_*` (0 FX … 13 OCT+), mapped through `panel.btn[]` |
| `web_keys(mask)` | held keys: bit k = key k (0 = F3 … 26 = G5) |
| `web_enc(role, n)` | signed detents. Roles: 0 SELECT, 1 ALGORITHM, 2 PRESETS, 3-6 KNOB1-4 |
| `web_master(v)` | MASTER, 0..1023 (the ADC scale; the firmware's own smoothing and curve apply) |
| `web_midi_usb(pkt)` | one USB-MIDI event packet (cable 0) → `midi_in_event` |
| `web_midi_trs(byte)` | one raw MIDI byte → `um_byte` |
| `web_mo_buf()`, `web_mo_w()` | MIDI-out ring: 256 × `uint32` packets, plus the write counter (the page keeps its own read counter) |
| `web_fb()`, `web_blits()` | 240×240 RGB565 framebuffer, byte-swapped as the panel takes it, plus its change counter |
| `web_lit_buttons()`, `web_lit_keys()` | lit LEDs: logical `B_*` bits and key bits |
| `web_flash()`, `web_flash_size()` | the simulated 1 MiB flash |
| `web_flash_dirty()`, `web_flash_clean()` | a 256-bit bitmap (8 × `uint32`) of 4 KiB sectors written since the last clean, and clearing it |
| `web_events()` | pending event bits, cleared by reading. Bit 0: the UBOOT combo was completed |
| `web_version()` | `char*` holding `FELUCCA_VERSION` |
| `web_ms()` | the device's milliseconds since boot |

**Messages between the page and the worklet:**
- **Page → worklet:** `load {wasm, flash}`, `buttons {mask}`, `keys {mask}`, `enc {role, n}`, `master {v}`, `midi {route: "usb"|"trs", bytes}`.
- **Worklet → page, about 30 Hz:** `frame {fb?, ledsBtn, ledsKey, midiOut[], events, flash?, load}`. `flash` is a list of dirty sectors as `{index, bytes}`, sent at most once a second.

**Rules:**
- The worklet runs at most one 128-frame render per `process()` call, and feeds queued MIDI in before rendering. That gives the ordering and latency the MIDI spec requires.
- SysEx never reaches the WASM. The page drops it and logs it.

### D7. Web MIDI adapter
- The page side calls `navigator.requestMIDIAccess({sysex: false})` lazily, the first time the selector is used.
- The chosen input id and route are saved in localStorage.
- **USB route:** each message is packed into a USB-MIDI event packet (cable 0, CIN from the status byte) and passed to `midi_in_event`.
- **TRS route:** the raw bytes go into `um_byte`, so running status and realtime interleaving are handled by the firmware's own parser.
- SysEx is never requested, so it never arrives. If a browser sends it anyway, it is filtered out and logged.
- **On disconnect:** the adapter sends note-offs for every note it saw held. It tracks per-channel note sets for this.

### D8. Modalities registry
The page has a `MODALITIES` table: `{id, label, state, describe(), adapter}`. The panel UI is generated from it. The adapters are:

- `midiIn`: Web MIDI (D7).
- `midiOut`: a log. The worklet captures packets at `midi_out_event`.
- `editorSysex`: a log. It records ignored input SysEx and any `ed_service` output.
- `usbAudio`: covered by the page output.
- `samples`, `backup`, `update`, `battery`, `console`: unavailable, each with a reason string.

Each adapter only uses the D6 messages, so it can be swapped alone, as the spec requires.

### D9. Unavailable modalities, firmware side
- `fm1_adc_read(FM1_ADC_BATT)` returns -1, which is the device's "no reading" path.
- The UBOOT and OTA requests can't happen: there is no USB host and `FELUCCA_OTA=0`.
- The UBOOT button combo is stubbed in `host_run_ms`: it shows the existing "UPDATE MODE" countdown text, then a page toast instead of `fm1_enter_uboot`.
- `fm1_delay_ms` is a no-op, and `fm1_wdt_feed` is a no-op.

## Risks / Trade-offs

- **[Risk] The host loop drifts from `main.c` / `audio.c` as the firmware changes.** → `hostapp_test` goldens, plus the allowed byte-identical refactors (D2). Reviewers check `sloop_host.c` whenever `main.c` changes.
- **[Risk] A `fm1_*` symbol is missing from `hal_host.h` and the link fails.** → The native `hostapp_test` build runs in `tests/run_tests.sh` and fails fast, before any WASM build.
- **[Risk] Browser performance.** The firmware's 8 voices × 4 tracks plus FX at 44.1 kHz may be heavy on phones. → The page shows the load %. The emulator never sheds voices because of browser timing. On slow devices it may drop out, and that is documented.
- **[Risk] Emscripten version drift changes the floating-point output.** → The emsdk image is pinned. `-ffp-contract=off`. Golden parity is checked in Node from the WASM itself (`test_web.mjs`).
- **[Trade-off] SysEx is disabled on Web MIDI input,** so a browser never prompts for SysEx permission, and the editor connection is not emulated.
- **[Risk] IndexedDB quota or private browsing.** → If persistence fails, the page says "not saved" and continues in memory.

## Migration Plan

Additive only. The device firmware, the installer and the editor are unchanged. Publishing adds `docs/webapp/emu/`. Rollback is deleting that folder and its README link.

## Open Questions

- The final keyboard mapping for SCL and the other layer buttons beyond the X0X set (a cosmetic choice, settled while testing the page).
- Whether the GitHub Pages deploy should rebuild the WASM in CI or commit `docs/webapp/emu/sloop.wasm`, as is done for the editor today. Either works with this design.
