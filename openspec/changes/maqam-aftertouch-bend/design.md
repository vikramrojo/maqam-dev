# Design

## Context

- **Hardware (from fm1-velocity, not yet confirmed on this fork):**
  - The keys are an 11 × 6 diode matrix behind two 74HC595s. Rows PA7 (ADC ch 0, notes 9..19) and PA8 (ch 1, notes 0..8 and OCT−/OCT+) are also SARADC inputs. PA6 (notes 20..26) has no ADC.
  - A 10 kHz TIMER5 tick scans one column per tick. A conversion is started after the column latch and collected on the next tick (~60 µs at divider 7, delay 2). Each analog key is sampled every 2.2 ms.
  - DRIVE: a held key's row is driven high at a weak pin drive (HD0/HD bits, level 2) every other sample. The reading falls by up to ~64 counts as the key is pressed harder; noise is about ±5 counts, so there are 10..13 usable steps before smoothing.
- **This fork today:**
  - `firmware/hal/fm1_input.h` (Felucca 1.0's scan, with dim-LED pulses in the tick) is digital only.
  - `main.c` reads the battery and MASTER pot with a blocking `fm1_adc_read` in the main loop.
  - Voices take a note's cents through `pitch24_set` / `bend_fine` (clamped ±100 c) and MIDI pitch bend through `bendq` in `track_render`.

## Goals / Non-Goals

- **Goals:**
  - a continuous, per-key microtonal bend from pressure;
  - zero behavioural change in a default build;
  - no change to any upstream format;
  - a small, isolated HAL diff.
- **Non-goals:** strike velocity, MPE, pressure on the top 7 keys, recording pressure.

## Decisions

### D1. Forward compatibility with SLOOP
- **Build flag.** Everything is behind `#if MAQAM_PRESS` (default 0, `core.h`; `MAQAM_PRESS=1 ./build.sh` turns it on). With the flag off, the preprocessed firmware holds none of the pressure code: the hooks expand to `((void)0)`, `PRESS_Q` to `0`, and no global, page, array or MQ byte is added (a host test checks the preprocessed `felucca.c`).
- **Two hook lines in upstream's file.** `fm1_input.h` gets `FM1_INPUT_SENSE_TAKE()` before the rows are read and `FM1_INPUT_SENSE_START(n)` after column n is latched, both empty unless `fm1_press.h` defines them. The DRIVE / ADC code lives in a new fork-only header, `firmware/hal/fm1_press.h`, included before `fm1_input.h` in `felucca.c`. Elsewhere the upstream files get one `#if MAQAM_PRESS` block each (`main.c`, `params.c`, `core.h`, `project.c`) and one macro term (`voice.c`, `seq.c`).
- **Formats.**
  - **Projects:** AFT goes in MQ block bytes 9 (value) and 10 (`~value`, its check). MQ stays version 1, and byte 8's check still covers bytes 0..7 only, so older fork builds read the block unchanged. Upstream ignores the whole block.
  - **Presets, settings and upstream magics:** unchanged.
  - **Editor:** AFT is a new global after G_INTON (in the test build only), sent through the existing parameter description. The protocol version stays 11.
- **MIDI.**
  - Poly pressure is sent only with AFT on, so with AFT OFF the MIDI stream is unchanged.
  - Incoming poly or channel pressure is ignored, as upstream does.
  - A learned CC can set AFT (fork-only, like INTON).

### D2. Pressure measurement
- The tick ports fm1-velocity's deferred conversion (start after the column latch, take on the next tick) plus the row drive and release, for the held keys 0..19 only.
- `fm1_press[k]` = clamp((landing − now) × 127 / RANGE, 0, 127):
  - landing = the lowest driven reading in the first 5 ms after the digital key-down;
  - RANGE = 64 counts (a constant at first; a device setting later if units differ).
- **The LED shift (~+25 counts):** the tick blanks the sampled row's key LED during the conversion. If that flickers visibly, the alternative is a fixed per-LED offset.
- **MASTER and the battery:** each gets a scan slot (the MASTER on (odd frame, column 0), as fm1-velocity does). The battery is read in another free slot every 64th frame. The blocking reads in `main.c` stay for builds without the flag.

### D3. Bend in the voice
- `press_q[track][voice]` (`pitch24.h`, not a `voice_t` field, so the default build's structures are unchanged), in 1/4096 semitone, is added to the pitch sum next to `bendq` in `track_render` (`PRESS_Q`). This avoids the ±100 c clamp of the note's own cents and rides on glide.
- `press.c`, every CTL block from `keyboard_block`: each held `KS_NOTE` key's pressure goes to the gated voices of its track sounding one of its notes (`kb_nt`): target = press × 25 c. One-pole smoothing (1/32 a block, ~23 ms). A voice whose age changed (a new note) starts from 0.
- Poly key pressure out every 8 blocks (~6 ms), for moves of 3 or more and the return to 0.
- With AFT OFF, the bends glide back to 0 and no message is sent.

### D4. Pressure and the OCT re-press
- A key re-pressed by OCT (`kb_repress`) keeps its landing, so its pressure carries on without a jump.

### D7. Flash: the test build has no parameter icons
- The default image is 581,308 B of the app's 581,564 B flash region (`firmware/app.ld`, XIP): 256 B spare. The pressure code needs ~2 KB, so the first CI build overflowed by 1,876 B.
- The test package is therefore built with `FELUCCA_ICONS=0` (upstream's switch: the labels get their full width, about 3 KB freed). The default package keeps the icons.
- Making AFT part of the default build needs that space found elsewhere first (smaller sample sets, or a larger app region, which moves the flash data area and is a format question).

### D5. One direction: up, 25 cents
- The reading can only fall below the key's landing (pressing harder), so the pressure is 0..127, never negative: a bend can go one way only. Up is chosen: a string pressed harder on a fretless neck goes sharp, and a vibrato that rises from the note keeps the note itself in tune at rest.
- Only the lowest range, 25 c: the noise (±5 counts in 64) gives 10..13 steps, ~2 c each at 25 c, below hearing; at 100 c they would be audible. AFT is therefore OFF / ON, not a depth.

### D6. The key's LED and the landing
- A lit key LED lifts the reading by ~25 counts, and a held key's LED lights a frame or so after the press, after the landing. So the key's own LED is dark for its sample tick: a held key on rows PA7 / PA8 glows at about half brightness in the test build.

## Risks / Trade-offs
- **[Risk] Breaking key scanning** (all keys look pressed if the rows' digital buffers are left off) → the flag defaults off; on-device checks come first. USB rescue (OCT− at power-on) still works, because it is read before the app starts.
- **[Risk] Tick overrun** with the dim-LED pulses → measure the tick on the device (`target_budget.py` for the ISR) before enabling.
- **[Risk] The feel is untested** (fm1-velocity lists aftertouch by feel as untested) → a separate test package; 25 c only.
- **[Trade-off] Only F3..C5 respond.** The keys above C5 play without bend. This is documented on the GLO page hint.
- **[Trade-off] Upstream merge friction** → kept to one hook line and one new header.

## Open Questions
- Should AFT be per track (an extra MQ byte per track) instead of global?
- After the device test: is 25 c the right depth, is up the right way (DOWN would be a second value), and is the half-bright held-key LED acceptable?
