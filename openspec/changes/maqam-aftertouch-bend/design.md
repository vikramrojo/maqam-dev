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
- **Build flag.** Everything in the HAL is behind `#if MAQAM_PRESS`. With the flag off, `build/felucca.fwsc` must be byte-identical to a build without this change (a test compares the SHA-256).
- **One hook in upstream's file.** `fm1_input.h` gets one call site, `FM1_INPUT_SENSE(col, rows)`, defined empty unless `MAQAM_PRESS`. The DRIVE / ADC code lives in a new fork-only header, `firmware/hal/fm1_press.h`. A merge of upstream's `fm1_input.h` then conflicts on at most one line.
- **Formats.**
  - **Projects:** AFT goes in MQ block bytes 9 (value) and 10 (`~value`, its check). MQ stays version 1, and byte 8's check still covers bytes 0..7 only, so older fork builds read the block unchanged. Upstream ignores the whole block.
  - **Presets, settings and upstream magics:** unchanged.
  - **Editor:** AFT is a new global after G_INTON, sent through the existing parameter description. The protocol version stays 11; an editor that doesn't know AFT shows it from its description.
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
- A new `int16_t at_q` in `voice_t`, in 1/4096 semitone, is added to the pitch sum next to `bendq` in `track_render`. This avoids the ±100 c clamp of the note's own cents and rides on glide.
- `at_q` is smoothed towards its target each block (one-pole, ~30 ms) and reset at `voice_start`.
- Every 5 ms, `seq.c` maps each held `KS_NOTE` key to its voices (`kb_nt[k][]` on track `kb_trk[k]`): target = press × depth × 4096 / (100 × 127).
- With AFT OFF, `at_q` stays 0, so renders are bit-identical (the golden renders guard this).

### D4. Pressure and the OCT re-press
- A key re-pressed by OCT (`kb_repress`) keeps its landing, so its pressure carries on without a jump.

## Risks / Trade-offs
- **[Risk] Breaking key scanning** (all keys look pressed if the rows' digital buffers are left off) → the flag defaults off; on-device checks come first. USB rescue (OCT− at power-on) still works, because it is read before the app starts.
- **[Risk] Tick overrun** with the dim-LED pulses → measure the tick on the device (`target_budget.py` for the ISR) before enabling.
- **[Risk] The feel is untested** (fm1-velocity lists aftertouch by feel as untested) → ship 25c as the first non-OFF value; try it on the device before making AFT visible by default.
- **[Trade-off] Only F3..C5 respond.** The keys above C5 play without bend. This is documented on the GLO page hint.
- **[Trade-off] Upstream merge friction** → kept to one hook line and one new header.

## Open Questions
- Should depth be per track (an extra MQ byte per track) instead of global?
- Should a negative depth (pressure bends down) be offered, for maqam's lowered approach notes?
