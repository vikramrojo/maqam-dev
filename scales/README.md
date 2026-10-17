# SLOOP scales

The 73 scales of SLOOP live in six plain text files, one per set: `west.txt`, `turk.txt`, `arab.txt`, `india.txt`, `psych.txt`, `jins.txt`. `tools/gen_scales.py` turns them into `build/gen/scales.h` (`SCALES[]`, `NSCALES`), and `tools/build.py` runs it with the other generators. Adding a scale is a text edit: no firmware source changes.

## A scale

```
# a comment
scale CODE "Name" home <note>
  from <value>        (optional) the scale's own tonic
  up   <values>       ascending form, the tonic first
  down <values>       descending form (optional: the same as up)
```

- **CODE**: at most 5 characters, unique over all scales. It is what the parameter pages and the web editor show. Distinguish look-alikes (`T.RST` for the Turkish Rast, `RAST` for the Arabic one).
- **"Name"**: at most 8 ASCII characters.
- **home**: the traditional tonic, a hint shown on the SCL screen (`home Bb`); choosing a scale never moves the root. A letter `A`..`G` and at most one suffix: `b` flat, `#` sharp, `d` half-flat, `+` half-sharp. `C`, `Bb`, `Ed` (E half-flat).
- **Values** are Scala (`.scl`) syntax, relative to the tonic: **cents** if the value has a decimal point (`386.`, `701.955`), otherwise a **ratio** (`9/8`, `5/4`; `2` alone is `2/1`). You can paste values from any `.scl` file.
- `from <value>` names the scale's own tonic when the values are written from another note (ARAB rows are written as ratios from Rast; Bayati says `from 9/8`). Every value is divided by it and reduced into one octave.
- 5 to 12 degrees, the tonic in both forms.

How the generator reads a value: it rounds it to the nearest quarter tone (50 cents) of the 24-note grid, which sets that degree in the scale's mask, and keeps the remainder (at most ±25 cents) as that degree's *tuning offset*. TRADITIONAL intonation adds the offset at note-on; EQUAL ignores it. Both forms of a scale share one offset per grid position, so the build fails if two pitches land on the same position with different offsets.

By set (the convention of the files, kept in each file's header comment):

- **WEST**: plain equal-tempered cents (`0. 200. 400. ...`), so every offset is 0.
- **TURK**: Pythagorean ratios (Arel-Ezgi-Uzdilek). Write comma counts only in comments.
- **ARAB**: al-Ṣabbāgh (1954) ratios from Rast.
- **INDIA**: just-intonation ratios.
- **PSYCH**: equal quarter-tone cents (`0. 150. 300. ...`), so every offset is 0: makam and maqam scales as King Gizzard and the Lizard Wizard play them on quarter-tone-fretted guitars. Five families, 4 + 3 + 3 + 3 + 3 (Hüseyni, Hijaz, subminor Phrygian, Bhairav: raga Bhairav's re and dha low, or neutral as Persian Chahargah, and Rast), each a 12-tone frame with two moving notes; the file header explains the slots.
- **JINS**: equal quarter-tone cents, like PSYCH: combination scales, a lower jins from one PSYCH family and an upper jins from another (KGLW's K.G.L.W. scale, the maqamat Nairuz, Suznak and Hijaz with Rast on the 4th, raga Darbari). Each row is attested; the file header lists the jinsas.

## Rules

- **Append only.** A scale's index is saved in projects, so existing scales never move. The generator holds the frozen list of the 73 shipped scales (`FROZEN` in `tools/gen_scales.py`): they keep their indices whatever the line order in the files, and removing or renaming one is a build error that names it. **Add a new scale to any set's file**: it gets the next free index (53, 54, ...) and belongs to that file's set; the SCL knob still steps through it with the rest of its set. Once a new scale ships, add its code to the end of `FROZEN`.
- **Cite the source** of every new row in a comment (a book, a page, a website, a recording): ratios and cents need to be checkable.
- The build fails, naming the file, line and scale, for: a value exactly halfway between two quarter-tone steps (the rounding would be ambiguous: nudge it by a cent or write it as a ratio), two different offsets on one grid position, a code over 5 characters or a repeated code, fewer than 5 or more than 12 degrees, or a tonic missing from a form.

## Check

```
python3 tools/gen_scales.py --print                  # a table: code, set, masks, offsets
python3 tools/gen_scales.py build/gen/scales.h       # write the header
python3 tests/scales_check.py                        # the checks and the error fixtures
```

Try a scale without touching the repository: copy this directory, edit the copy, and generate from it with `--dir`:

```
cp -r scales /tmp/mine
printf '\nscale TEST "TestScl" home D   # throwaway\n  up   1/1 9/8 5/4 4/3 3/2 5/3 15/8\n' >> /tmp/mine/india.txt
python3 tools/gen_scales.py /tmp/scales.h --dir /tmp/mine
```

`NSCALES` is then 74. The frozen test in `tests/scale_test.c` pins the shipped 73 and will say so; a scale you keep needs its row added there and its code to `FROZEN` in the generator.
