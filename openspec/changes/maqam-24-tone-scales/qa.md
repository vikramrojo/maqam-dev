# QA: maqam-24-tone-scales (on SLOOP 2.6)

This is the manual testing for this change, now merged onto **SLOOP 2.6**. Sections 2–5 close the open tasks: **8.1** (the device build) and **8.2** (on the FM-1). Section 1 is an optional dry run in the emulator, which catches most problems before you flash anything. For the screens, see [`SCALES.md`](../../../SCALES.md). Tick boxes as you go, fill in the log at the end, and send Claude the results so it can tick `tasks.md`.

**Already passing on the host (no toolchain needed):**
- **182 golden renders unchanged** against upstream 2.6's own `golden.txt`, so 12-tone sound is identical to SLOOP 2.6.
- **1302 host checks pass.** That covers scales, keys, the sequencer, CHORD+ on 24-tone scales, CENT locks, the FUN6 + MQ project block, user presets, the UI pages, the scale generator, the web editor (protocol 11) and the emulator.
- **One known failure:** one CPU-budget entry (`PHASE/03_RESO_PLUCK`, +25 %) fails here. It fails the same way on clean upstream 2.6 on this computer.

**Not yet run:**
- anything that needs the JieLi device compiler;
- anything that needs the FM-1;
- the editor's built-in audio preview (task 9.4: it needs `emcc`, so the preview is still upstream's build, without CENT).

---

## 0. Before you start

- [ ] On the FM-1, as it is now, open the web editor and choose **Projects tab → Save a backup**. Keep the file.
  - Projects stay in SLOOP 2.6's own format (FUN6). Going back to SLOOP 2.6 keeps your projects; they play in 12-tone there.
  - The backup is still the safe way back, and it's the test data for section 3.
- [ ] Note one or two projects whose sound you know well: their key (root), scale and tempo. Section 3 checks they come back identical.

---

## 1. Dry run in the emulator (optional, about 10 minutes)

The emulator runs the merged firmware in a browser. Build it and serve it:

```sh
sh web/emu/build.sh
cp build/emu/index.html build/emu/sloop.wasm docs/webapp/emu/
python3 -m http.server 8000 --bind 127.0.0.1 -d docs/webapp/emu
```

Open **http://localhost:8000/** in Chrome and press **Switch on**. Use the panel or the keyboard:
- **`.`** holds SEL, and **`=`** holds SEQ.
- **`Z`** / **`X`** are OCT− / OCT+.
- The white keys are **`Q`–`I`** then **`A`–`K`**, from F3: so C4 is **`T`**, D4 `Y`, E4 `U`, F4 `I`, B4 `D`, C5 `F`.
- Shift-click latches a button.

Run section 4 here first. Anything that fails here will also fail on the FM-1, and it's quicker to report from here.

---

## 2. Device build (closes task 8.1)

**Needs:**
- Docker Desktop, or Colima with Rosetta;
- the JieLi toolchain (`tools/get_toolchain.sh`);
- the AC79 SDK, as in `BUILDING.md`.

```sh
./build.sh                  # build/felucca.fwsc; also regenerates build/gen/scales.h
tests/run_tests.sh          # the full suite, including the parts that need the device build
```

- [ ] `./build.sh` succeeds with **no new warnings** in `pitch24.h`, `seq.c`, `voice.c`, `project.c`, `editor.c`, `gfx.c` or `ui_*.c`. It's the first time the merged code meets the JieLi clang 4.0.1. If it fails, send Claude the error lines.
- [ ] `build.py`'s checks pass:
  - at least **8 KiB of the pool** is still free;
  - the RAM and section sizes fit.

  Copy the size summary into the log.
- [ ] `tests/run_tests.sh` ends with `ALL HOST TESTS PASSED`. Check these steps in particular, since they couldn't run here:
  - `M-UPGRADE entry`;
  - `update loader`;
  - `target cost of the render loops`;
  - `the offline site`;
  - `the site without internet` (needs Playwright).
- [ ] If the only failure is the CPU budget on a fast computer, compare with clean upstream 2.6 (`git stash`, or a worktree at `main`). `target_budget.py` is the check that matters for the FM-1.

---

## 3. Install, and projects from SLOOP 2.6 (part of task 8.2)

1. Install `build/felucca.fwsc` with the web installer (served from `localhost`).
2. [ ] The FM-1 boots to the TRACKS screen. **About** (hold HOME → about) shows the new build.
3. [ ] **The work in progress survives.** The project you were working on plays and sounds the same.
4. [ ] **2.6 projects load unchanged.** Load each of projects 1–4. Each has:
   - the same key: hold SEL and read the title bar;
   - the same scale and tempo;
   - CENT at 0 (SEL 2 page, KNOB 4).

   It plays exactly as before. The title bar ends in `TRAD`.
5. [ ] **An old backup restores.** In the editor, use **Projects tab → Restore from a file** with the section 0 backup. It loads without errors and plays the same.
6. [ ] **A fork project round-trips.** Make a project with a quarter-tone root (section 4.3), EQUAL tuning and one CENT lock (section 4.5). Save it, power-cycle, and load it: everything comes back.
7. [ ] **Optional, going back to upstream:** flash upstream SLOOP 2.6 and load that project. It opens without errors and plays in 12-tone; quarter tones become plain semitones, and the CENT lock is gone. Then reinstall this build.

---

## 4. Microtonal checks on the FM-1 (closes task 8.2)

Use a synth track (1–3) with a clear sound (a sine-like lead, no glide). Keep the sequencer stopped unless a step says otherwise.

### 4.1 Arabic Rast, up and down
1. Hold **SEL**, then:
   - press **PRESETS** two steps clockwise (WEST → TURK → ARAB), so the title bar shows `ARAB`;
   - turn **KNOB 2** to `RAST`;
   - press the **C** key;
   - set **KNOB 3 (KEYS) = WHITE**.
2. Let go of SEL. Play the white keys from **C4 up to C5**, then back **down**.
3. [ ] Going **up**, the 7th degree is **B half-flat**: lower than B, higher than B♭.
4. [ ] Coming **down**, the same key gives **B♭**.
5. [ ] **Record and read it back.** Press REC, play the run, and stop. Hold **SEQ**: the tiles show B with the half-flat glyph going up and B♭ coming down.
6. [ ] With **KEYS = SNAP**, the **E♭** and **E** keys both play E half-flat.
7. [ ] With **INDIA `ASAVR`** (Asavari) in WHITE mode, play C4–C5 up and down. Going up repeats two notes (D, A♭), and going down plays all seven. No key jumps to a different place in the scale.

### 4.2 Turkish Segah vs Kürdi, and EQUAL / TRADITIONAL
1. Hold **SEL**. Use **PRESETS** to get `TURK`, and set the root to C.
2. Choose `KURDI` and hold the **D** key (2nd degree). Then choose `SEGAH` and play D again.
3. [ ] In **TRADITIONAL** (`TRAD`), the 2nds differ by about **24 c**: 114 c vs 90 c above C.
4. [ ] Hold SEL and turn **ALGORITHM** left: the title shows `EQUAL`, and the two 2nds now sound the same. Turn right for `TRAD`.
5. [ ] **WEST `MAJ`** sounds the same in both settings.

### 4.3 Any scale on any root, and the home hint
1. [ ] Hold SEL and choose **ARAB `AJAM`** with root **C**. The root stays C, and the label over KNOB 2 reads `HOME BB` (B♭).
2. [ ] Hold **OCT−** and press **E** while SEL is held. The root becomes **E half-flat** (E with the half-flat glyph). Choose `SIKAH`: it plays from E half-flat.
3. [ ] Change scales with KNOB 2. The root never changes on its own.

### 4.4 Quarter-tone modifier and the octave buttons
1. [ ] With **WEST `CHR`**, hold **OCT−** and press **E**. You hear **E half-flat**, and the octave is unchanged afterwards.
2. [ ] Hold **OCT+** and press **E**: you hear **E half-sharp**.
3. [ ] Tap **OCT+** alone: the octave goes up **when you let go**. Both OCT buttons together reset the octave.
4. [ ] On the **drum track (4)**, OCT− and OCT+ still play ghost and hard hits.
5. [ ] Holding both OCT buttons for 5 s, stopped, still shows the update-mode countdown. Let go before it ends.
6. [ ] **CHORD+** (SEL 2: STRUM, VLEAD) with chords on in ARAB Rast: chords strum and voice-lead. The chords on E and B are built on E half-flat and B half-flat.

### 4.5 CENT: track fine tune and step bends (locks)
1. [ ] Tap **SEL** twice for **SEL 2**. **KNOB 4 CENT** at −30 makes the whole track audibly flat. Set it back to 0.
2. [ ] **A step bend.** Make a short pattern, then:
   - hold **SEQ** and hold the key of step 1;
   - turn **ALGORITHM** until the title reads **LOCK CENT**;
   - turn **PRESETS** to about **−30**.

   Let go: step 1's tile has a small dot.
3. [ ] Press PLAY. Step 1 is flatter, and the next step is back in tune. A chord step bends all its notes.
4. [ ] With the step held, **OCT−** clears its locks, and the bend is gone.
5. [ ] **NOTE editing.** On the STEP page (tap SEQ), hold OCT− and turn **KNOB 2 NOTE** one detent: the note moves a quarter tone. Without OCT, it moves by semitones and keeps its quarter tone.

### 4.6 MIDI out and in
1. [ ] Play **E half-flat** with a MIDI monitor connected: MIDI out shows **D#** (63) for both note-on and note-off. That includes the sequencer's own MIDI out, if it's enabled (GLO > MIDI).
2. [ ] Notes sent **into** the FM-1, over USB or TRS, play plain 12-tone, even with a TURK or ARAB scale in TRADITIONAL. That includes 2.6's step entry from a MIDI keyboard and its per-track MIDI channels.
3. [ ] **MIDI pitch bend** (new in 2.6) bends a quarter-tone note smoothly, on top of its quarter tone and CENT.

### 4.7 Web editor (protocol 11)
1. [ ] Connect the editor: it reports protocol **11**.
2. [ ] Type **`Ed4`** in a step and send it. On the FM-1, the step shows E half-flat.
3. [ ] A CENT lock made on the FM-1 shows in the editor's step lock list as **CENT** with its value, and one made in the editor plays on the FM-1.
4. [ ] **SCALE** lists **52** scales, ROOT has **24** values, and **INTON** (EQUAL / TRAD) is in the globals.
5. [ ] **Save a backup**, then restore it: the quarter tones and CENT locks come back.

---

## 5. Musical sanity (optional, but the point of the change)

Ask someone who plays maqam, makam or raga to try a few scales, in TRADITIONAL and EQUAL:
- Arabic: Rast, Bayati, Sikah, Hijaz;
- Turkish: Rast, Uşşak, Segah, Hicaz;
- Indian: Yaman, Bhairav.

Note anything that sounds wrong. Fixes are data edits in `scales/*.txt`. Also check the al-Ṣabbāgh ratios in `scales/arab.txt` against the printed 1954 source.

---

## Log (fill in and send back)

```
Date / build (About screen):
Section 2  ./build.sh:             ok / errors (paste)
           pool free, sizes:
           run_tests.sh:           ALL PASSED / which failed
           target_budget:          within / over (which)
Section 3  2.6 projects / backup:  same / differences
           fork project round trip / on upstream 2.6:
Section 4  4.1 Rast up/down, Asavari:   ok / notes
           4.2 Segah vs Kurdi, EQUAL:   ok / notes
           4.3 any root, home hint:     ok / notes
           4.4 OCT modifier, CHORD+:    ok / notes
           4.5 CENT + locks:            ok / notes
           4.6 MIDI out/in, pitch bend: ok / notes
           4.7 editor v11:              ok / notes
Section 5  musician feedback:
Anything else (e.g. the HOME BB label):
```

Once sections 2–4 pass, Claude ticks 8.1 and 8.2. Tasks 9.4 (preview rebuild) and 9.6 (GUIDE.md and the French quick start) are still open. Then `/opsx:archive` finishes `maqam-24-tone-scales`.
