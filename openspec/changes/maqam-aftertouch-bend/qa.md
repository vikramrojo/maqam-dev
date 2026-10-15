# QA: maqam-aftertouch-bend (the test package)

**Download:** the CI run's artifact `maqam-press-firmware-<sha>`, file `maqam-press.fwsc`. Install it with the web installer as usual, after a backup. Its About screen (hold HOME → about) reads `MAQAM 2.6 AFT`. To go back, install the normal `maqam-firmware-<sha>` package.

## 1. Nothing broke
- [ ] Every key from F3 to G5 plays once per press: no stuck notes, no double notes, no notes on their own.
- [ ] The buttons and the knobs work, OCT− / OCT+ included.
- [ ] **MASTER** changes the volume smoothly.
- [ ] The battery icon shows a sensible level (unplugged, on battery).
- [ ] Held keys on the lower two thirds of the keyboard (F3..C5) glow at about half brightness. That's expected in this build.
- [ ] Hold **OCT− + OCT+** for 5 s, stopped: the update-mode countdown still appears. Let go before it ends.

## 2. The bend
1. Tap **GLO** until the **PRESS** page shows (after TEXTURE 2). Turn **KNOB 1** (**AFT**) to `ON`.
2. Pick a sustained POLY sound on a synth track (a pad or an organ).
3. [ ] Hold A4 and press it harder: the note rises a little, up to about a quarter of a semitone, and comes back as you ease off.
4. [ ] Rock the pressure: it sounds like a vibrato. Note whether it is too subtle, too strong, steppy or noisy.
5. [ ] Hold a chord in chord mode: the whole chord moves with the key's pressure.
6. [ ] Keys above C5 (C#5..G5) never bend.
7. [ ] AFT `OFF`: no key bends.
8. [ ] With a MIDI monitor: poly key pressure messages appear while you press (AFT ON only).

## Log
```
Build (About):
Nothing broke (keys, MASTER, battery, UBOOT):
The bend: depth ok / too little / too much; smooth / steppy; direction up ok / prefer down:
LED half brightness acceptable:
```
