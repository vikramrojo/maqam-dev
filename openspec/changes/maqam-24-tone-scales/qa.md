# QA: maqam-24-tone-scales

This is the manual testing left for this change. Sections 2–5 close the two open tasks: **8.1** (the device build) and **8.2** (on the FM-1). Section 1 is an optional dry run in the emulator, which catches most problems before you flash anything. Tick boxes as you go, fill in the log at the end, and send Claude the results so it can tick `tasks.md`.

**Already passing on the host (no toolchain needed):**
- All 97 golden renders are unchanged, so 12-tone sound is bit-identical to SLOOP 2.3.
- 621 host checks pass: scales, keys, sequencer, UI pages, the FUN5 projects and importers, user presets, the scale generator and its negative fixtures.
- The web-editor tests pass, including protocol v7.
- The emulator rebuilds.

**Not yet run:** anything that needs the JieLi device compiler, and anything that needs the FM-1 itself.

---

## 0. Before you start

- [ ] On the FM-1 **still running SLOOP 2.3**, open the web editor and choose **Projects tab → Save a backup**. Keep the file. Two reasons:
  - The new firmware saves projects as **FUN5**, which SLOOP 2.3 and older can't read.
  - This backup is the only way back, and it's also the test data for section 3.
- [ ] Note one or two projects whose sound you know well: their key (root), scale and tempo. Section 3 checks they come back identical.

---

## 1. Dry run in the emulator (optional, about 10 minutes)

The emulator runs the new firmware in a browser. Build it and serve it:

```sh
sh web/emu/build.sh
cp web/emu/index.html build/emu/sloop.wasm docs/webapp/emu/
python3 -m http.server 8000 --bind 127.0.0.1 -d docs/webapp/emu
```

Open **http://localhost:8000/** in Chrome and press **Switch on**. Use the panel, or the keyboard:
- **`.`** holds SCL, **`=`** holds SEQ.
- **`Z`** / **`X`** are OCT− / OCT+.
- The white keys are **`Q`–`I`** then **`A`–`K`**, so C4 is **`U`**.
- Shift-click latches a button.

Run sections 4.1 to 4.5 here first. Anything that fails here will fail on the FM-1, and it's quicker to report from here.

---

## 2. Device build (closes task 8.1)

**Needs:** Docker Desktop (or Colima with Rosetta), the JieLi toolchain (`tools/get_toolchain.sh`) and the AC79 SDK, as in `BUILDING.md`.

```sh
./build.sh                  # builds build/felucca.fwsc; also regenerates build/gen/scales.h
tests/run_tests.sh          # the full suite, including the parts that need the device build
```

- [ ] `./build.sh` succeeds with **no new warnings** in `pitch24.h`, `seq.c`, `voice.c`, `project.c`, `editor.c` or `ui_*.c`. This is the first time the new code meets the JieLi clang 4.0.1; if it fails, send Claude the error lines.
- [ ] `build.py`'s own checks pass: at least **8 KiB of the memory pool is still free**, and the RAM and section sizes fit. Copy the size summary it prints into the log.
- [ ] `tests/run_tests.sh` ends with `ALL HOST TESTS PASSED`. The steps that couldn't run here are new to check:
  - `M-UPGRADE entry` (`ota_test`);
  - `update loader: other app -> this build` (`ldr_test`);
  - `regression: target cost of the render loops` (`target_budget.py`), which must be within budget.
- [ ] If only the CPU lines fail on a fast computer (`3 CPU entries over budget`), that's machine noise, as before this change. `target_budget.py` is the check that matters for the FM-1.

---

## 3. Install, and older projects (part of task 8.2)

1. Install `build/felucca.fwsc` with the web installer (`INSTALL-SLOOP.bat`, or `docs/webapp/installer/` served from `localhost`).
2. [ ] The FM-1 boots to the TRACKS screen. The **About** screen (hold HOME → about) shows the new build.
3. [ ] **The work in progress survives.** The project you were working on before the update is still there, plays, and sounds the same.
4. [ ] **Saved projects load.** Load each of projects 1–4 (SAVE layer). Each has:
   - the same key: hold SCL and read the title bar;
   - the same scale and tempo;
   - every step's bend at 0: STEP 2 → CENT shows 0.

   The music plays exactly as before.
5. [ ] **The tuning setting.** Hold SCL. The title bar ends in `trad`, the default for projects from before.
6. [ ] **Restoring an old backup.** In the editor, use **Projects tab → Restore from a file** with the backup from section 0. It loads without errors, and the projects play the same.
7. [ ] **Saving in the new format.** Save one project again (SAVE layer), power-cycle the FM-1, and load it. It comes back identical.

---

## 4. Microtonal checks on the FM-1 (closes task 8.2)

Use a synth track (1–3) with a clear sound, e.g. a sine-like lead with no glide. Keep the sequencer stopped unless a step says otherwise.

### 4.1 Arabic Rast, going up and down
1. Hold **SCL**.
   - Press **PRESETS** two steps clockwise (WEST → TURK → ARAB). The title bar shows `arab`.
   - Turn **KNOB 2** to `rast`.
   - Press the **C** key to set the root.
   - Set **KNOB 3 (keys) = WHITE**.
2. Let go of SCL. Play the white keys from **C4 up to C5**, then back **down to C4**.
3. [ ] Going **up**, the 7th degree is **B half-flat**: noticeably lower than a normal B, and higher than B♭.
4. [ ] Coming **down**, the same key gives **B♭**.
5. [ ] **Record and read it back.** Press REC, play the same up-and-down run, and stop. Hold **SEQ**: the step tiles show the half-flat glyph (a reversed flat) on the B going up and a normal flat B coming down. The 3rd degree shows E with the half-flat glyph both ways.
6. [ ] **SNAP** (KNOB 3 = SNAP): with the same scale and root, the **E♭** key and the **E** key both play E half-flat.

### 4.2 Turkish Segah vs Kürdi, and the EQUAL switch
1. Hold **SCL**. Use **PRESETS** to get `turk`, and set root **C**.
2. Turn KNOB 2 to `kurdi`, play and hold the **2nd degree** (WHITE mode: the D key). Then switch to `segah` and play the same key.
3. [ ] In **TRADITIONAL** (title bar `trad`), the two 2nds are audibly different: Segah's is about 24 c higher (114 c vs 90 c above C), a slow beat if you play them against each other on two tracks.
4. [ ] Hold SCL and turn **ALGORITHM** left. The title bar says `equal`, and now Kürdi's and Segah's 2nds sound **the same**. Turn it right again and it says `trad`.
5. [ ] **A Western scale is unaffected.** Choose WEST `maj`: switching between `trad` and `equal` changes nothing you can hear.

### 4.3 Any scale on any root, and the home hint
1. Hold **SCL**. Set **ARAB** `ajam` with root **C**.
2. [ ] The root stays **C**: Ajam plays as a major-like scale from C. Under KNOB 2 the hint reads `home Bb`.
3. [ ] Hold **OCT−** and press **E** while SCL is held. The root becomes **E half-flat**: the title bar starts with E and the half-flat glyph. Choose `sikah`, and it plays from E half-flat.
4. [ ] Change scales with KNOB 2. The root never changes on its own.

### 4.4 The quarter-tone modifier and the octave buttons
1. Hold SCL and choose **WEST `chr`** (chromatic) with KEYS = OFF or SNAP.
2. [ ] Hold **OCT−** and press **E**. You hear **E half-flat**. Let go: **the octave has not changed** (play a key to compare).
3. [ ] Hold **OCT+** and press **E**. You hear **E half-sharp**.
4. [ ] **Tap OCT+ alone** (no key while it's held). The octave goes up by one **when you let go**. Tap OCT− and it comes back down.
5. [ ] Press **both** OCT buttons together: the octave resets to 0.
6. [ ] On the **drum track (4)**, OCT− and OCT+ still play ghost and hard hits, as before.
7. [ ] The **UBOOT combo** is unchanged: holding both OCT buttons for 5 s while stopped still shows the update-mode countdown. Let go before it ends.

### 4.5 Bends on steps
1. Record a short pattern on a synth track (or enter steps with **SEQ** held and the keys).
2. Hold **SEQ** and hold a step key. On the **STEP** page, turn **KNOB 2 NOTE** one detent while holding **OCT−**.
   - [ ] The note moves by a **quarter tone**, and the tile shows the half-flat glyph.
   - [ ] Turning NOTE without OCT moves it by semitones and keeps the quarter tone (E half-flat + 2 = F# half-flat).
3. Tap **SEQ** again to get **STEP 2**. Hold a step and turn **KNOB 2 CENT** to about **−30**.
   - [ ] The tile shows `−30c`, or `−30` when the label is long.
4. [ ] Press PLAY: that step plays audibly flatter. A chord step bends **all** its notes.
5. [ ] **Save** the project, power-cycle, and load it. The bends and quarter tones come back.

### 4.6 MIDI out and in
1. Connect the FM-1 by USB to a computer running a MIDI monitor (e.g. MIDI Monitor on macOS, or the DAW's MIDI input view).
2. [ ] Play **E half-flat** on the keys. MIDI out shows **D#** (note 63) for both note-on and note-off. No note hangs.
3. [ ] Send notes **into** the FM-1 from a keyboard or DAW (USB or the TRS jack). They play plain 12-tone pitches, even with a TURK or ARAB scale and TRADITIONAL on.

### 4.7 Web editor (protocol v7)
1. Open the web editor, connect, and go to the step table of a synth track.
2. [ ] The editor reports protocol **7**, and the step table has a **Cents** column.
3. [ ] Type **`Ed4`** in a step's note and **+7** in Cents, then send. Hold SEQ on the FM-1: that step shows E half-flat with `+7c`, and plays slightly sharp of E half-flat.
4. [ ] A step bent on the FM-1 (section 4.5) shows its cents in the editor.
5. [ ] **SCALE** in the editor lists **52** scales; ROOT has **24** values; the last global is **INTON** (EQUAL / TRAD).
6. [ ] **Save a backup** again. It now holds FUN5 projects.

---

## 5. Musical sanity (optional, but the point of the change)

Ask someone who plays maqam, makam or raga to try a few scales, in TRADITIONAL and EQUAL:
- Arabic: Rast, Bayati, Sikah, Hijaz;
- Turkish: Rast, Uşşak, Segah, Hicaz;
- Indian: Yaman, Bhairav.

Note anything that sounds wrong: a note, a descending form, or an offset. Fixes are data edits in `scales/*.txt`. Also check the al-Ṣabbāgh ratios in `scales/arab.txt` against the printed 1954 source (design.md, Risks).

---

## Log (fill in and send back)

```
Date / build (About screen):
Section 2  ./build.sh:             ok / errors (paste)
           pool free, sizes:
           run_tests.sh:           ALL PASSED / which failed
           target_budget:          within / over (which)
Section 3  old projects / backup:  same / differences
Section 4  4.1 Rast up/down:       ok / notes
           4.2 Segah vs Kurdi:     ok / notes
           4.3 any root, home:     ok / notes
           4.4 OCT modifier:       ok / notes
           4.5 step bends:         ok / notes
           4.6 MIDI out/in:        ok / notes
           4.7 editor v7:          ok / notes
Section 5  musician feedback:
Anything else:
```

Once sections 2–4 pass, Claude ticks 8.1 and 8.2. `/opsx:archive` then finishes `maqam-24-tone-scales`, and `sloop-web-emulator` too once its own device-build check is done; the device build in section 2 covers it.
