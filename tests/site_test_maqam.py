#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""maqam: the site as Maqam (web/make_site.py + tools/brand_site.py) on a fake package: the landing page, no SLOOP in
what a visitor reads (except the compatibility section and the credits), the functional strings untouched, the
manifest, the icons, sw.js keeping the emulator and the renamed firmware, and a brand pass that fails when an
upstream wording changes."""
import json
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "web"))
import make_site  # noqa: E402

bad = 0


def check(what, ok):
    global bad
    print(f"maqam site: {what:<78} {'ok' if ok else 'FAIL'}")
    bad += not ok


# what may keep the name "sloop": file names and ids code depends on, identifiers, the font family, the credits
KEEP = [r"sloop-backup", r"sloop-drumsynth", r"sloop-seq-view", r"sloop-emu(?:-midi)?", r"sloop-site-", r"sloop-editor(?:-[\w.]*)?\.html",
        r"sloop\.webmanifest", r"sloop-preview(?:\.wasm)?", r"sloop\.wasm", r"SLOOP Terminus", r"SLOOP-FONT", r"\.sloopdrums\.json",
        r"felucca\|sloop\|", r"felucca\|sloop/i", r'"sloop"', r"\b[Ss]loop[A-Z]\w*", r"\bSloop\b",
        r'<a href="https://github\.com/isod89/sloop-fm1"[^>]*>SLOOP</a>', r"github\.com/isod89/sloop-fm1"]


def visible_leftovers(text, landing=False):
    t = re.sub(r"data:[^\"')\s]*", "", text)
    t = re.sub(r"<!--.*?-->", "", t, flags=re.S)
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    t = re.sub(r"(?m)(^|\s)//.*$", r"\1", t)
    if landing:
        t = re.sub(r'<section[^>]*id="sloop".*?</section>', "", t, flags=re.S)
    for k in KEEP:
        t = re.sub(k, " ", t)
    return [t[max(0, m.start() - 50):m.end() + 40].replace("\n", " ") for m in re.finditer(r"(?i)sloop", t)]


def png_size(p):
    b = p.read_bytes()
    return struct.unpack(">II", b[16:24]) if b[:8] == b"\x89PNG\r\n\x1a\n" else None


tmp = Path(tempfile.mkdtemp())
pkg = bytearray(4096)
for i, ch in enumerate("FM-1_900"):
    pkg[i * make_site.BLK + make_site.KEEP] = (ord(ch) + i + 1) & 0xFF
for i in range(8, make_site.BLOCKS):
    pkg[i * make_site.BLK + make_site.KEEP] = 0x7D
pkg[2000:2016] = b"FELUCCA-LOADER-1"
(tmp / "p.fwsc").write_bytes(bytes(pkg))
site = tmp / "site"
make_site.main(tmp / "p.fwsc", "9.9", site)
if not (site / "webapp" / "emu" / "index.html").exists():          # (web/emu/build.sh not run: a stand-in)
    (site / "webapp" / "emu").mkdir(exist_ok=True)
    (site / "webapp" / "emu" / "index.html").write_text((ROOT / "web" / "emu" / "index.html").read_text(encoding="utf-8"), encoding="utf-8")
    (site / "webapp" / "emu" / "sloop.wasm").write_bytes(b"\0asm\1\0\0\0")
    sw = (site / "webapp" / "sw.js").read_text(encoding="utf-8")
    (site / "webapp" / "sw.js").write_text(sw.replace('"icon-192.png"', '"emu/", "emu/sloop.wasm", "icon-192.png"'), encoding="utf-8")
before = {p: p.read_bytes() for p in site.rglob("*") if p.is_file()}
r = subprocess.run([sys.executable, str(ROOT / "tools" / "brand_site.py"), str(site)], capture_output=True, text=True)
print(r.stdout.strip(), r.stderr.strip())
check("brand_site.py runs on a site from make_site.py", r.returncode == 0)

app = site / "webapp"
land = (site / "index.html").read_text(encoding="utf-8")
check("root index.html is the landing page, not a redirect", "http-equiv" not in land and "<title>Maqam" in land)
check("landing: install, emulator, guide and scales cards (no editor)", all(f'href="{h}"' in land for h in ("webapp/installer/", "webapp/emu/", "webapp/guide/", "webapp/scales/")))
check("landing: links to the guide and the scales pages, and the source", 'href="webapp/guide/"' in land and 'href="webapp/scales/"' in land and 'href="https://github.com/vikramrojo/maqam-dev"' in land)
m = re.search(r'<section[^>]*id="sloop".*?</section>', land, re.S)
check("landing: one 'Forward compatible with SLOOP' section with the credits",
      land.count("Forward compatible with SLOOP") == 1 and m and all(s in m.group(0) for s in ("FUN6", "Felucca", "Leo Kuroshita", "GPL-3.0", "isod89", "Rakkas", "Terminus", "fm1-x0x", "Largen")))
check("landing: self-contained (no http script, no CDN)", not re.search(r'<script[^>]+src=|<link[^>]+href="https?:', land))
for f in ["index.html", "webapp/installer/index.html", "webapp/editor/index.html", "webapp/editor/sloop-editor.html",
          "webapp/emu/index.html", "webapp/editor/preview.js", "webapp/sloop.webmanifest", "webapp/guide/index.html", "webapp/scales/index.html"]:
    left = visible_leftovers((site / f).read_text(encoding="utf-8"), landing=(f == "index.html"))
    check(f"no SLOOP left in {f}", not left)
    for x in left[:5]:
        print("    ", x)
inst = (app / "installer" / "index.html").read_text(encoding="utf-8")
ed = (app / "editor" / "index.html").read_text(encoding="utf-8")
emu_html = (app / "emu" / "index.html").read_text(encoding="utf-8")
check("the editor keeps its Maqam wordmark image; the other pages have the Rakkas wordmark link",
      'alt="Maqam"' in ed and all('<a class="wordmark"' in x for x in (inst, emu_html, land)))
check("installer and emulator wordmarks link back to the landing page", 'href="../../" title="Maqam"' in inst and 'href="../../" title="Maqam"' in emu_html)
# functional strings, unchanged
check("kept: MIDI port patterns", "/fm-1|felucca|sloop|clipper|ota|composite|sinco|usb-midi/i" in inst + ed and "/felucca|sloop/i" in ed)
check("kept: backup / drum kit / library / patch format ids",
      all(s in ed for s in ('"sloop-backup"', '"sloop-drumsynth"', '"felucca-library"', '"felucca-patch"')))
check("kept: storage keys and the font", all(s in ed for s in ('"sloop-seq-view"', '"felucca-editor"', "SLOOP Terminus", "/*SLOOP-FONT*/")))
check("kept: file names code refers to", 'href="sloop-editor.html"' in ed
      and "sloop-preview.wasm" in (app / "sw.js").read_text(encoding="utf-8") + (app / "editor" / "preview.js").read_text(encoding="utf-8"))
check("kept: the package identity checks", "FM-1_9" in inst and "FELUCCA-LOADER" in (ROOT / "web" / "make_site.py").read_text(encoding="utf-8"))
check("kept: emulator storage keys and worklet name", all(s in (app / "emu" / "index.html").read_text(encoding="utf-8") for s in ('"sloop-emu"', '"sloop-emu-midi"', 'registerProcessor("sloop"', "sloop.wasm")))
# the guide and the scales pages
guide = (app / "guide" / "index.html").read_text(encoding="utf-8")
imgs = re.findall(r'<img src="(img/[^"]+)"', guide)
check("guide: Getting started + the SCALES.md text, at least 10 images that resolve",
      "Getting started" in guide and "The KEY layer" in guide and len(imgs) >= 10 and all((app / "guide" / i).is_file() for i in imgs))
check("guide: no repo-relative link left (they point at the repository)", not re.search(r'href="(?!https?:|#|\.\./|mailto:)[^"]*"', guide.replace('href="../../"', "")))
sc = (app / "scales" / "index.html").read_text(encoding="utf-8")
codes = re.findall(r'<tr class="up"><th scope="row"><code>([^<]+)</code>', sc)
sys.path.insert(0, str(ROOT / "tools"))
import gen_scales as G  # noqa: E402
import gen_site_pages as P  # noqa: E402
rows = P.scales_data()
check("scales: exactly the 73 scales of the data file, in order", codes == G.FROZEN and len(codes) == 73)
check("scales: 16 / 12 / 12 / 12 / 16 / 5 per set", [sum(1 for r in rows if r["set"] == s) for s in range(6)] == [16, 12, 12, 12, 16, 5] and sc.count("<table") == 6)
trad = lambda code, k, form="up": [r for r in rows if r["code"] == code][0][form][k][1]   # noqa: E731
check("scales: TRAD cents (TURK Rast 3rd 384.4, ARAB Rast 3rd 356.9, INDIA Bhupali Ga 386.3)",
      abs(trad("T.RST", 2) - 384.4) < 0.06 and abs(trad("RAST", 2) - 356.9) < 0.06 and abs(trad("BHUPL", 2) - 386.3) < 0.06
      and "384.4" in sc and "356.9" in sc and "386.3" in sc)
check("scales: down rows exactly where the masks differ", sc.count('<tr class="down">') == sum(1 for r in rows if r["asc"] != r["desc"]) > 5)
check("scales: half-flat / half-sharp spelled with d / +", '<sup class="q">d</sup>' in sc and '<sup class="q">+</sup>' in sc)
# the Markdown converter
md = P.md_to_html("# T\n\npara **b** *i* `c` [l](u) ![a](x.png)\n\n| A | B |\n|---|---|\n| ![i](y.png) | 2 |\n\n- one\n  1. two\n     - three\n\n```\n<x>\n```\n\n---\n")
check("md converter: headings, inline, images, tables of images, nested lists, code, rules",
      all(s in md for s in ("<h1", "<strong>b</strong>", "<em>i</em>", "<code>c</code>", '<a href="u">l</a>', '<img src="x.png"', "<table>",
                            '<td><img src="y.png"', "<ul><li>one<ol><li>two<ul><li>three", "<pre>&lt;x&gt;</pre>", "<hr>")))
# one theme, one header / nav / footer on every page except the editor
pages = {"landing": land, "installer": inst, "emulator": emu_html, "guide": guide, "scales": sc}
theme = ["largen.css", "largen.components.css", "theme-dark.css", "maqam-theme.css", "maqam.css"]
check("every page links the shared theme and is data-theme=dark",
      all('data-theme="dark"' in h and all(f'assets/{c}"' in h for c in theme) for h in pages.values()))
check("the emulator page in the site uses ../assets/ (build/emu keeps assets/)", '../assets/largen.css' in emu_html
      and 'href="assets/' not in emu_html and 'href="assets/' in (ROOT / "web" / "emu" / "index.html").read_text(encoding="utf-8"))
navs = {k: re.findall(r">([^<]+)</a>", re.search(r'<nav class="site-nav".*?</nav>', h, re.S).group(0)) for k, h in pages.items()}
check("the same header navigation on every page, without the editor", len({tuple(v) for v in navs.values()}) == 1
      and list(navs["landing"]) == ["Home", "Install", "Try it", "How to play", "The 73 scales", "Source"])
foot = {k: re.sub(r"\s+", " ", re.search(r"<footer class=\"site-footer.*?</footer>", h, re.S).group(0)) for k, h in pages.items()}
check("the same footer on every page", len(set(foot.values())) == 1)
check("installer behaviour kept: the install flow elements and the script",
      all(f'id="{i}"' in inst for i in ("go", "bar", "status", "log", "lang", "stock-go", "stock-saved", "stock-file", "app"))
      and '<script type="module">' in inst and "FM1_PORT" in inst and "applyLang" in inst and "<style>" not in inst)
hrefs = {p: re.findall(r'href="([^"]*)"', p.read_text(encoding="utf-8")) for p in site.rglob("*.html") if "editor" not in p.parts}
check("no page except the editor links to the editor (any html: href with editor/)", not any("editor" in h for hs in hrefs.values() for h in hs))
check("the editor page and its offline copy say noindex,nofollow",
      all('<meta name="robots" content="noindex,nofollow">' in (app / "editor" / f).read_text(encoding="utf-8") for f in ("index.html", "sloop-editor.html")))
check("largen files and licence in webapp/assets, with the Rakkas font and its licence",
      all((app / "assets" / f).is_file() for f in theme + ["installer.css", "LARGEN-LICENSE.txt", "Rakkas-Regular.ttf", "RAKKAS-OFL.txt"])
      and "MIT License" in (app / "assets" / "LARGEN-LICENSE.txt").read_text(encoding="utf-8"))
mf = json.loads((app / "sloop.webmanifest").read_text(encoding="utf-8"))
check("manifest: name 'Maqam'; the app starts at the installer, not the hidden editor",
      mf["name"] == "Maqam" and mf["short_name"] == "Maqam" and "Maqam" in mf["description"] and mf["start_url"] == "installer/" and mf["id"] == "installer/")
sw = (app / "sw.js").read_text(encoding="utf-8")
files = json.loads(re.search(r"const FILES = (\[.*?\]);", sw).group(1))
check("sw.js lists emu/ (and the emulator's wasm)", "emu/" in files and "emu/sloop.wasm" in files and "emu/index.html" not in files)
check("firmware renamed to maqam-VER.fwsc: file, installer META, sw.js", (site / "firmware" / "maqam-9.9.fwsc").is_file()
      and not list((site / "firmware").glob("sloop-*")) and '"pkg": "../../firmware/maqam-9.9.fwsc"' in inst and "../firmware/maqam-9.9.fwsc" in files)
path = lambda f: (app / f / "index.html") if f.endswith("/") else (app / f)   # noqa: E731
check("sw.js: every file it keeps is in the site", all(path(f).is_file() for f in files))
import hashlib  # noqa: E402
h = hashlib.sha256()
for k in sorted(files):
    h.update(k.encode() + b"\0" + path(k).read_bytes())
check("sw.js: the stamp is the hash of the branded files", f'-{h.hexdigest()[:12]}"' in sw)
check("icons: PNG 192, 512 and maskable 512", png_size(app / "icon-192.png") == (192, 192) and png_size(app / "icon-512.png") == (512, 512)
      and png_size(app / "icon-maskable-512.png") == (512, 512) and (app / "icon-192.png").read_bytes() != before[app / "icon-192.png"])
# a changed upstream wording fails the pass, naming the entry; a second run on a branded site fails too
r2 = subprocess.run([sys.executable, str(ROOT / "tools" / "brand_site.py"), str(site)], capture_output=True, text=True)
check("brand_site.py refuses a site that is already branded (names the entry)", r2.returncode == 1 and "expected" in r2.stderr)
site2 = tmp / "site2"
make_site.main(tmp / "p.fwsc", "9.9", site2)
f = site2 / "webapp" / "installer" / "index.html"
f.write_text(f.read_text(encoding="utf-8").replace("<title>SLOOP installer</title>", "<title>SLOOP flasher</title>"), encoding="utf-8")
r3 = subprocess.run([sys.executable, str(ROOT / "tools" / "brand_site.py"), str(site2)], capture_output=True, text=True)
check("brand_site.py fails when an upstream string changed (exit 1, file named)", r3.returncode == 1 and "installer/index.html" in r3.stderr)
print("maqam site test", "FAILED" if bad else "passed")
sys.exit(1 if bad else 0)
