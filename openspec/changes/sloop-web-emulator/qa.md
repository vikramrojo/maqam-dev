# QA: sloop-web-emulator

These are the manual checks left after the automated work. Each section ends with the task it closes. Tick the boxes here as you go, then tell Claude which tasks passed (or paste what failed) so it can tick `tasks.md`.

Automated checks already passing: `tests/hostapp_test`, `node web/test_web.mjs`, `node web/emu/test_page.mjs`, the UI test, the 97 golden renders, and a headless-Chrome sweep (screenshots in `build/emu/verify/`).

---

## 0. Start the emulator (every section needs this)

From the repo root:

```sh
sh web/emu/build.sh                                       # only if firmware or web/emu C changed
cp web/emu/index.html build/emu/sloop.wasm docs/webapp/emu/   # keep the published copy current
python3 -m http.server 8000 --bind 127.0.0.1 -d docs/webapp/emu
```

Open **http://localhost:8000/** (or http://127.0.0.1:8000/).

> **Not** the `http://[::]:8000/` link that `http.server` prints without `--bind`, and not `0.0.0.0` or your LAN IP. On those addresses the browser treats the page as insecure and turns off audio worklets and Web MIDI. The page now says so instead of doing nothing.

- [ ] Click **Switch on**. The status reads `On.`, the version reads `Firmware: SLOOP 2.3`, and the screen shows the SLOOP tracks page with HOME lit.

If it doesn't start, copy the red "Could not start: …" line, plus any errors in DevTools → Console, and send them to Claude.

**Testing on another device (iPad, phone):** a LAN address like `http://192.168.x.x:8000/` is insecure, so audio won't start. Either publish to GitHub Pages (https), or use Chrome's remote debugging / Safari Web Inspector to forward `localhost`.

---

## 1. Panel and input in three browsers (closes task 3.1)

Do all of the following in **Chrome**, **Firefox** and **Safari**. Safari has no Web MIDI: the MIDI selector shows "Web MIDI not supported in this browser", and everything else must still work.

| Check | Chrome | Firefox | Safari |
|---|---|---|---|
| Switch on works; you hear sound when you hold a key (`Q`) | [ ] | [ ] | [ ] |
| `Q` held: the leftmost white key lights (F3) and sound holds until you let go | [ ] | [ ] | [ ] |
| Drag a knob up/down and use the scroll wheel on it: the screen value changes in single steps | [ ] | [ ] | [ ] |
| **Shift-click SCL**: it stays latched (outlined). Click a white key: the KEY layer screen shows (key tiles, root lit). Shift-click SCL again to release it | [ ] | [ ] | [ ] |
| Right-click a button also latches it (and no context menu appears) | [ ] | [ ] | [ ] |
| Keyboard map: `Space` PLAY, `Enter` REC, `Z`/`X` OCT−/OCT+, `Esc` HOME, `↑`/`↓` SELECT (tempo) | [ ] | [ ] | [ ] |
| **Three-finger chord on touch**: three keys sound at once (iPad, or DevTools device mode with multi-touch) | [ ] | — | [ ] |

Notes on failures:

---

## 2. A real MIDI keyboard (closes task 4.2)

Use Chrome or Firefox. Firefox asks for a site permission, and may need a MIDI add-on prompt the first time.

1. Plug in a MIDI keyboard, then choose it in **MIDI in**. Allow the permission. The status reads `connected: <name>`.
2. **USB route** (the default):
   - [ ] Channel 1, 2 and 3 notes play synth parts 1, 2 and 3. Most keyboards send on channel 1; change the keyboard's channel to test 2 and 3.
   - [ ] Channel 10, note 36 (C1) plays the drum kick.
   - [ ] A channel the firmware doesn't map, e.g. 5, plays the **selected** track.
   - [ ] Fast playing and big chords leave no stuck or missing notes.
3. **TRS route**: switch **Route as** to TRS.
   - [ ] Notes still play. Ideally use a sequencer or DAW that sends running status (most send note-ons with velocity 0 as note-offs); every note must sound and stop.
4. [ ] The **MIDI out** log shows what the firmware echoes from the panel keys (MIDI notes coming in are not echoed out, as on the device).
5. [ ] Hold a key on the MIDI keyboard and unplug it. The status reads `disconnected` and no note hangs. Plug it back in: it reconnects.
6. [ ] Reload the page. The keyboard is selected again (Chrome remembers the permission).

---

## 3. A ten-minute play session (closes task 7.2)

Chrome, the MIDI keyboard from section 2, and a DAW or drum machine sending MIDI clock (e.g. Ableton Live or Logic with "MIDI Clock" sync out to the same port or an IAC bus).

1. On the emulator: GLO → SYSTEM → **SYNC = USB**, with the route on USB.
2. Start the DAW at 100 BPM. The emulator starts and follows the tempo. Stop the DAW: the emulator stops.
3. Record a pattern: REC, play the keyboard for a few bars. Add a drum pattern with SEQ held and the white keys.
4. Let it run while you change sounds and use FX / punch-ins for about 10 minutes.
5. Press SAVE and keep a project. Reload the page and switch on: the project and pattern are back. Play it again.
6. Optionally, try **Factory reset**. It asks first, then boots empty.

Session log (fill in):

```
Date / browser / OS:
Keyboard / DAW:
Clock followed (start, stop, tempo):      yes / no
Stuck notes:                              none / describe
Screen glitches:                          none / describe
Dropouts (with "% of this computer's time" shown):
Saved, reloaded, same project:            yes / no
Anything else:
```

---

## 4. The device build is unchanged (closes tasks 1.3 and 7.1)

This needs the JieLi toolchain (see `BUILDING.md`: Docker on macOS, plus the AC79 SDK at `~/fw-AC79_AIoT_SDK`).

```sh
git stash push -u -m qa-emu-baseline     # or check out the commit before this change in a second worktree
./build.sh && shasum -a 256 build/felucca.fwsc > /tmp/before.sha
git stash apply                           # restore the emulator change (then drop the stash entry)
./build.sh && shasum -a 256 build/felucca.fwsc
diff <(cut -d' ' -f1 /tmp/before.sha) <(shasum -a 256 build/felucca.fwsc | cut -d' ' -f1) && echo IDENTICAL
tests/run_tests.sh
```

- [ ] `IDENTICAL` is printed. `firmware/` was not edited, so this is expected. If the hashes differ, check whether `build.sh` embeds a date or version; if it does, compare `build/felucca.bin` instead.
- [ ] `tests/run_tests.sh` passes. It now also builds and runs `hostapp_test`. Lines saying CPU is over budget are machine-speed noise when golden renders show 0 changed.

---

## What to send back

- Which boxes passed, per section.
- For any failure: the browser, the steps, the console errors, and the session log.

Claude then ticks tasks 1.3, 3.1, 4.2, 7.1 and 7.2. Once those are ticked, the change is ready for `/opsx:archive`.

---

## Results

- **2026-10-06:** the user ran the manual checks in **Chrome** and reported that everything works. Tasks 3.1, 4.2 and 7.2 are ticked on that basis. **Firefox, Safari and real multi-touch are still unverified**: run section 1's other columns before relying on those browsers. Safari's no-Web-MIDI start path was checked in headless Chrome with Web MIDI removed.
- **Section 4 is deferred** (no Docker or Colima, JieLi toolchain or AC79 SDK on this machine). Tasks 1.3 and 7.1 stay open. `firmware/` has no changes in this change (`git diff --quiet -- firmware`), so the device build is expected to be identical.
