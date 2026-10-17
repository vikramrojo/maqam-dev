#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Checks tools/gen_scales.py output against a hard-coded copy of the design.md appendix
(openspec/changes/maqam-24-tone-scales), the WEST masks of seq.c, and the negative fixtures."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_scales as G  # noqa: E402

# (code, asc, desc or None, tune) -- transcribed from the appendix (TURK: Pythagorean AEU ratios)
EXP = """
T.RST 444511 144511 4:+4 8:-16 10:-2 14:+2 18:+6 20:-4 22:-14
USSAK 114451 - 4:-20 6:-6 10:-2 14:+2 16:-8 20:-4
HUSEY 144451 114451 4:-20 6:-6 10:-2 14:+2 16:-8 18:-18 20:-4
KURDI 114445 - 2:-10 6:-6 10:-2 14:+2 16:-8 20:-4
NIHAV 414451 114451 4:+4 6:-6 10:-2 14:+2 16:-8 20:-4 22:-14
HICAZ 144505 114505 2:+14 8:-16 10:-2 14:+2 16:-8 18:-18 20:-4
HCKAR 414505 - 2:+14 8:-16 10:-2 14:+2 16:+16 22:-14
SUZIN 414511 - 4:+4 8:-16 10:-2 14:+2 16:+16 22:-14
KARCI 141451 - 4:-20 6:-6 10:-2 12:+12 18:-18 20:-4
T.SBA 114151 - 4:-20 6:-6 8:-16 14:+2 16:-8 20:-4
SEGAH 114445 - 2:+14 6:+18 10:+22 14:+2 16:+16 20:+20
HUZZM 114245 - 2:+14 6:+18 9:-19 14:+2 16:+16 20:+20
RAST 244491 144491 4:+4 7:+7 10:-2 14:+2 18:+6 20:+20 21:+9
BAYAT 114449 - 3:+3 6:-6 10:-2 14:+2 16:+16 20:-4
SABA 114149 - 3:+3 6:-6 8:-14 14:+2 16:+16 20:-4
SIKAH 224889 222889 3:-9 7:-5 11:-1 13:+13 14:+2 17:-7 21:-3
HIJAZ 124505 114505 2:+12 8:-14 10:-2 14:+2 16:+16 17:+5 20:-4
NAHAW 414451 114451 4:+4 6:+16 10:-2 14:+2 16:+16 20:+20 22:-12
KURD 114445 - 2:+12 6:-6 10:-2 14:+2 16:+16 20:-4
AJAM 444511 - 4:-20 8:-16 10:-4 14:-22 18:-18 22:-14
NIKRZ 145051 - 4:+4 6:+16 12:+12 14:+2 18:+6 20:+20
HJKAR 414505 - 2:+12 8:-14 10:-2 14:+2 16:+16 22:-12
SUZNK 414491 144491 4:+4 7:+7 10:-2 14:+2 16:+16 18:+6 20:+20 22:-12
HUZAM 228289 222889 3:-9 7:-5 9:+9 11:-1 13:+13 15:-19 17:-7 21:-3
YAMAN 441111 445111 4:+4 8:-14 12:-10 14:+2 18:-16 22:-12
BILAW 444511 - 4:+4 8:-14 10:-2 14:+2 18:-16 22:-12
KHMAJ 444501 144511 4:+4 8:-14 10:-2 14:+2 18:-16 20:-4 22:-12
KAFI 144451 - 4:+4 6:+16 10:-2 14:+2 18:-16 20:-4
ASAVR 014411 114451 4:+4 6:+16 10:-2 14:+2 16:+14 20:-4
BHRVI 114445 - 2:+12 6:+16 10:-2 14:+2 16:+14 20:-4
BHAIR 414505 - 2:+12 8:-14 10:-2 14:+2 16:+14 22:-12
TODI 415045 - 2:+12 6:+16 12:-10 14:+2 16:+14 22:-12
PURVI 415105 - 2:+12 8:-14 12:-10 14:+2 16:+14 22:-12
MARWA 441105 - 2:+12 8:-14 12:-10 18:-16 22:-12
BHUPL 044111 - 4:+4 8:-14 14:+2 18:-16
MALKS 110441 - 6:+16 10:-2 16:+14 20:-4
"""
# seq.c SCALE_MASK, 12-bit (bit i -> 24-bit bit 2i)
WEST12 = [0xFFF, 0xAB5, 0x5AD, 0x6AD, 0x6B5, 0x295, 0x4A9, 0x9AD, 0x5AB, 0xAD5, 0x56B, 0xAAD, 0x4E9, 0x555,
          0x6DB, 0xB6D]
HOMES = {"RAST": "C", "BAYAT": "D", "SABA": "D", "SIKAH": "Ed", "HIJAZ": "D", "NAHAW": "C", "KURD": "D",
         "AJAM": "Bb", "NIKRZ": "C", "HJKAR": "C", "SUZNK": "C", "HUZAM": "Ed", "T.RST": "G", "USSAK": "A",
         "HUSEY": "A", "KURDI": "A", "NIHAV": "G", "HICAZ": "A", "HCKAR": "G", "SUZIN": "G", "KARCI": "A",
         "T.SBA": "A", "SEGAH": "Bd", "HUZZM": "Bd"}

PSYCH_HOMES = {"P.HUS": "A", "P.MSR": "D", "P.NEV": "A", "P.USK": "A", "P.HJZ": "D", "P.HKR": "G", "P.NHK": "G",
               "P.SUB": "G", "P.SU6": "G", "P.SEE": "Gb", "P.BHL": "C", "P.CHG": "C",
               "P.HMY": "C", "P.RST": "C", "P.ACR": "G", "P.MHR": "C"}
JINS_HOMES = {"J.KGL": "B", "J.NRZ": "C", "J.SZK": "C", "J.HJR": "D", "J.DRB": "C"}


def spread(m):
    return sum(1 << (2 * i) for i in range(12) if m >> i & 1)


def main():
    res = G.load(ROOT / "scales")
    by = {r["code"]: r for r in res}
    bad = 0

    def chk(ok, msg):
        nonlocal bad
        if not ok:
            bad += 1
            print("FAIL:", msg)
    chk(len(res) == 73 and [r["code"] for r in res] == G.FROZEN, "73 codes in frozen order")
    chk([r["set"] for r in res] == G.EXPECT_SET, "set of each scale")
    chk(all(len(r["code"]) <= 5 for r in res) and len({r["code"] for r in res}) == 73, "codes")
    for r, m12 in zip(res[:16], WEST12):
        chk(r["asc"] == r["desc"] == spread(m12), f"{r['code']} mask {r['asc']:06X} vs seq.c {spread(m12):06X}")
        chk(not any(r["tune"]) and r["home"] == 0, f"{r['code']} tune/home")
    for line in EXP.strip().splitlines():
        f = line.split()
        code, asc, desc = f[0], int(f[1], 16), int(f[2], 16) if f[2] != "-" else int(f[1], 16)
        tune = [0] * 24
        for t in f[3:]:
            s, v = t.split(":")
            tune[int(s)] = int(v)
        r = by[code]
        chk(r["asc"] == asc, f"{code} asc {r['asc']:06X} != {asc:06X}")
        chk(r["desc"] == desc, f"{code} desc {r['desc']:06X} != {desc:06X}")
        chk(r["tune"] == tune, f"{code} tune {r['tune']} != {tune}")
        if code in HOMES:
            chk(r["home"] == G.home_pc(HOMES[code]), f"{code} home")
    for c in [x for x in by if by[x]["set"] == 3]:
        chk(by[c]["home"] == 0, f"{c} home")
    # PSYCH: equal quarter tones, no offsets; homes as written in psych.txt
    for c, h in PSYCH_HOMES.items():
        chk(by[c]["set"] == 4 and not any(by[c]["tune"]), f"{c} set / tune")
        chk(by[c]["home"] == G.home_pc(h), f"{c} home")
    for c, h in JINS_HOMES.items():
        chk(by[c]["set"] == 5 and not any(by[c]["tune"]) and by[c]["home"] == G.home_pc(h), f"{c} set / tune / home")
    # negative fixtures: must fail, naming the scale
    for sub, who in (("removed", "MIN"), ("collide", "MAJ")):
        d = ROOT / "tests" / "scales_bad" / sub
        p = subprocess.run([sys.executable, str(ROOT / "tools" / "gen_scales.py"), "--print", "--dir", str(d)],
                           capture_output=True, text=True)
        chk(p.returncode == 1 and who in p.stderr, f"fixture {sub}: rc={p.returncode} stderr={p.stderr!r}")
        print(f"fixture {sub}: {p.stderr.strip()}")
    print("scales_check:", "FAIL" if bad else "ok", f"({bad} problems)" if bad else "")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
