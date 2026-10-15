#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""maqam: brand the generated site (the output of web/make_site.py) as Maqam.

  tools/brand_site.py SITE_DIR

The upstream web sources (editor.html, index_pkg.html, fm1ota.js, sw.js, preview*.js) are NOT edited, so SLOOP
merges stay easy; this build-time pass rewrites the *displayed* text of the generated site instead:

  * a table of (files, old text, new text, expected count) replacements. A count that differs fails the run
    (exit 1) naming the entry, so an upstream change of wording is noticed here, not on the site;
  * the Maqam wordmark (Rakkas, SIL OFL, assets/fonts/rakkas) as an inline PNG instead of the SLOOP logos, and
    the PWA icons (webapp/icon-*.png) in Maqam's look;
  * the landing page (tools/brand_site_index.html) as the root index.html, with the "Forward compatible with
    SLOOP" section and the credits;
  * the firmware download renamed firmware/sloop-VER.fwsc -> maqam-VER.fwsc (installer META, sw.js list), and
    sw.js's stamp recomputed (it hashes the files, which this pass changed).

Left as they are, on purpose (they are functional, not display): the MIDI port patterns (/fm-1|felucca|sloop|.../),
the backup / library / drum-kit format ids ("sloop-backup", "sloop-drumsynth", "felucca-library", "felucca-patch"),
the storage keys ("sloop-seq-view", "felucca-editor*", "sloop-emu*"), sw.js's CACHE prefix "sloop-site-", the file
names code refers to (sloop-editor.html, sloop.webmanifest, sloop-preview.wasm, sloop.wasm), the AudioWorklet
names, the "SLOOP Terminus" font family, the /*SLOOP-FONT*/ marker, the package identity (FM-1_9xx,
FELUCCA-LOADER-1), the ".sloopdrums.json" file extension, and every code comment.
"""
import base64
import shutil
import hashlib
import io
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import gen_site_pages as P  # noqa: E402
head_links, site_header, site_footer = P.head_links, P.site_header, P.site_footer
FONT = ROOT / "assets" / "fonts" / "rakkas" / "Rakkas-Regular.ttf"
VENDOR = ROOT / "web" / "site" / "vendor" / "largen"
SITE_CSS = ROOT / "web" / "site"
INK, BG = (242, 242, 242), (0, 0, 0)

EDITORS = "webapp/editor/*.html"      # index.html and the offline copy sloop-editor.html
INSTALLER = "webapp/installer/index.html"
EMU = "webapp/emu/index.html"
IMG = ('<img alt="Maqam" src="{src}" style="display:block;height:auto" width="{w}" height="{h}">')


class Fail(Exception):
    pass


# ------------------------------------------------------------------ images --- #
def wordmark(size=132):
    """'Maqam' in Rakkas, light on transparent, tightly cropped -> (PNG bytes, w, h)"""
    from PIL import Image, ImageDraw, ImageFont
    f = ImageFont.truetype(str(FONT), size)
    l, t, r, b = f.getbbox("Maqam")
    pad = size // 10
    im = Image.new("RGBA", (r - l + 2 * pad, b - t + 2 * pad), INK + (0,))
    ImageDraw.Draw(im).text((pad - l, pad - t), "Maqam", font=f, fill=INK + (255,))
    out = io.BytesIO()
    im.save(out, "PNG", optimize=True)
    return out.getvalue(), im.width, im.height


def icon(px, maskable):
    """a Rakkas 'M' on black; maskable: inside the safe zone (the central 80 %)"""
    from PIL import Image, ImageDraw, ImageFont
    im = Image.new("RGB", (px, px), BG)
    box = px * (0.5 if maskable else 0.62)
    size = int(px * 0.6)
    while True:
        f = ImageFont.truetype(str(FONT), size)
        l, t, r, b = f.getbbox("M")
        if max(r - l, b - t) <= box or size < 8:
            break
        size -= 2
    d = ImageDraw.Draw(im)
    d.text(((px - (r - l)) / 2 - l, (px - (b - t)) / 2 - t - px * 0.02), "M", font=f, fill=INK)
    sw = px // 64 * 2 or 2                      # the four track colours as a strip under the letter
    y = int(px * (0.80 if maskable else 0.84))
    x0, x1 = int(px * (0.30 if maskable else 0.22)), int(px * (0.70 if maskable else 0.78))
    for k, c in enumerate(((40, 124, 255), (30, 204, 112), (255, 198, 24), (255, 98, 26))):
        d.rectangle([x0 + (x1 - x0) * k // 4, y, x0 + (x1 - x0) * (k + 1) // 4 - 1, y + sw], fill=c)
    out = io.BytesIO()
    im.save(out, "PNG", optimize=True)
    return out.getvalue()


def data_uri(png):
    return "data:image/png;base64," + base64.b64encode(png).decode()


# ------------------------------------------------------------------- table --- #
# (glob, old, new, expected count); a str is exact, a re.Pattern a regex
def table(logo_img):
    X = lambda g, a, b, n=1: (g, a, b, n)
    re_ = lambda p, f=re.S: re.compile(p, f)
    both = lambda a, b, n=1: [X(EDITORS, a, b, n)]
    T = []
    # ---- installer: re-skinned on the shared theme (the page's own <style> goes; its markup, ids and script stay)
    T += [X(INSTALLER, "<title>SLOOP installer</title>", "<title>Maqam installer</title>"),
          X(INSTALLER, '<html lang="en">', '<html lang="en" data-theme="dark">'),
          X(INSTALLER, re_(r"<style>.*?</style>"), head_links("../", installer=True)),
          X(INSTALLER, re_(r'<main>\s*<div class="top"><div class="logo">.*?</div><button class="lang" id="lang"></button></div>\s*'
                           r'<div class="strip"><i></i><i></i><i></i><i></i></div>'),
            site_header("../../", "../", None) + '\n<main class="page installer center stack">\n  <div class="top"><button class="lang" id="lang"></button></div>'),
          X(INSTALLER, "</main>", "</main>\n" + site_footer("../../")),
          # no way to the editor from here: the editor is a page of its own that nothing links to
          X(INSTALLER, re_(r'\s*<a class="ed" href="\.\./editor/" data-t="editor"></a>'), ""),
          X(INSTALLER, re_(r'\s*<p class="small"><a href="\.\./editor/sloop-editor\.html"[^>]*data-t="edOffline"></a></p>'), ""),
          X(INSTALLER, re_(r'\s*<p><a class="ed" href="\.\./editor/" data-t="stockEditor"></a></p>'), ""),
          X(INSTALLER, "SLOOP is based on Felucca by Leo Kuroshita (@kurogedelic), ",
            'Maqam is a fork of <a href="https://github.com/isod89/sloop-fm1" target="_blank" rel="noopener">SLOOP</a> '
            '(isod89), based on Felucca by Leo Kuroshita (@kurogedelic), '),
          X(INSTALLER, "SLOOP is not affiliated with them.", "Maqam is not affiliated with them."),
          X(INSTALLER, "say(`SLOOP ${meta.version}`)", "say(`Maqam ${meta.version}`)"),
          # ja
          X(INSTALLER, "公式ファームウェアは SLOOP の曲・", "公式ファームウェアは Maqam の曲・"),
          X(INSTALLER, "SLOOP を入れ直せば", "Maqam を入れ直せば"),
          X(INSTALLER, "SLOOP の曲は公式ファームウェアでは開けません", "Maqam の曲は公式ファームウェアでは開けません"),
          X(INSTALLER, "FM-1 は SLOOP の書き込みモードです。先に「インストール」で SLOOP の書き込みを終えて",
            "FM-1 は Maqam の書き込みモードです。先に「インストール」で Maqam の書き込みを終えて"),
          # en
          X(INSTALLER, "The official firmware cannot open SLOOP's music, projects or samples.",
            "The official firmware cannot open Maqam's music, projects or samples."),
          X(INSTALLER, "reinstall SLOOP and restore it", "reinstall Maqam and restore it"),
          X(INSTALLER, "The official firmware cannot open SLOOP's music. Did you save a backup?",
            "The official firmware cannot open Maqam's music. Did you save a backup?"),
          X(INSTALLER, "The FM-1 is in SLOOP's update mode. Press Install to finish the SLOOP update first",
            "The FM-1 is in Maqam's update mode. Press Install to finish the Maqam update first"),
          # no mention of the editor (a page of its own that nothing links to)
          X(INSTALLER, "Save a backup with the editor first (Projects → Backup); reinstall Maqam and restore it from the editor whenever you want them back.",
            "Keep a backup of what you want before you return to it; reinstall Maqam and restore it whenever you want them back."),
          X(INSTALLER, "先にエディタでバックアップを保存してください（Projects → Backup）。Maqam を入れ直せば、エディタから復元できます。",
            "先にバックアップを取っておいてください。Maqam を入れ直せば、復元できます。"),
          X(INSTALLER, "Install the app (the editor and the installer offline, Android too)", "Install the app (the installer offline, Android too)"),
          X(INSTALLER, "（エディタとインストーラーがオフラインで使える・Android も）", "（インストーラーがオフラインで使える・Android も）"),
          # the firmware download
          X(INSTALLER, re_(r'"pkg": "\.\./\.\./firmware/sloop-([^"]*)\.fwsc"'), r'"pkg": "../../firmware/maqam-\1.fwsc"')]
    # ---- editor (index.html and the offline copy)
    T += [X(EDITORS, "<title>SLOOP Editor</title>", "<title>Maqam Editor</title>"),
          # the editor is a secret page: nothing links to it, and search engines are told to leave it alone
          X(EDITORS, '<meta name="viewport" content="width=device-width, initial-scale=1">',
            '<meta name="viewport" content="width=device-width, initial-scale=1">\n<meta name="robots" content="noindex,nofollow">'),
          X(EDITORS, re_(r'(<a class="brand" href="#sound" aria-label=)"SLOOP">\s*<svg viewBox="44 24 572 186".*?</svg>'),
            r'\1"Maqam">' + logo_img),
          X(EDITORS, ".brand svg { width: 132px; height: auto; display: block; }",
            ".brand svg, .brand img { width: 132px; height: auto; display: block; }"),
          X(EDITORS, ".brand svg { width: 96px; }", ".brand svg, .brand img { width: 96px; }"),
          X(EDITORS, '<footer>SLOOP, based on Felucca by Leo Kuroshita (@kurogedelic), ',
            '<footer>Maqam, a fork of <a href="https://github.com/isod89/sloop-fm1" target="_blank" rel="noopener">SLOOP</a> '
            '(isod89), based on Felucca by Leo Kuroshita (@kurogedelic), '),
          X("webapp/editor/index.html", re_(r'download="sloop-editor-([^"]*)\.html"'), r'download="maqam-editor-\1.html"'),   # (the offline copy has no link)
          # ja
          X(EDITORS, 'nodevice: "SLOOP が見つかりません。', 'nodevice: "Maqam が見つかりません。'),
          X(EDITORS, 'installer: "SLOOP インストーラー"', 'installer: "Maqam インストーラー"'),
          X(EDITORS, 'bkBad: "SLOOP のバックアップファイル', 'bkBad: "Maqam のバックアップファイル'),
          X(EDITORS, 'bkNewer: "新しい SLOOP のバックアップです', 'bkNewer: "新しい Maqam のバックアップです'),
          X(EDITORS, "（SLOOP 2.4 以降）", "（Maqam 2.4 以降）", 2),
          X(EDITORS, "（SLOOP 2.5 以降）", "（Maqam 2.5 以降）", 2),
          X(EDITORS, 'dsFileBad: "SLOOP のドラムシンセキット', 'dsFileBad: "Maqam のドラムシンセキット'),
          # en
          X(EDITORS, 'nodevice: "SLOOP not found.', 'nodevice: "Maqam not found.'),
          X(EDITORS, 'installer: "SLOOP installer"', 'installer: "Maqam installer"'),
          X(EDITORS, 'bkBad: "Not a SLOOP backup, or damaged: "', 'bkBad: "Not a Maqam backup, or damaged: "'),
          X(EDITORS, "This backup comes from a newer SLOOP: update", "This backup comes from a newer Maqam: update"),
          X(EDITORS, "(SLOOP 2.4 or later)", "(Maqam 2.4 or later)", 1),
          X(EDITORS, "(SLOOP 2.4 and later)", "(Maqam 2.4 and later)", 2),
          X(EDITORS, "(SLOOP 2.5 or later)", "(Maqam 2.5 or later)", 2),
          X(EDITORS, 'dsFileBad: "Not a SLOOP drum synth kit"', 'dsFileBad: "Not a Maqam drum synth kit"'),
          X(EDITORS, 'throw new BackupError("bkBad", "not a SLOOP backup")', 'throw new BackupError("bkBad", "not a Maqam backup")'),
          X(EDITORS, 'throw new Error("not a SLOOP drum synth kit")', 'throw new Error("not a Maqam drum synth kit")'),
          X(EDITORS, '"FELUCCA SLOOP 2.6 (MOCK)"', '"FELUCCA MAQAM 2.6 (MOCK)"'),   # (?mock=1)
          # file names and the exported track name (display only)
          X(EDITORS, "`sloop-track${tr + 1}.mid`", "`maqam-track${tr + 1}.mid`"),
          X(EDITORS, "`SLOOP track ${tr + 1}`", "`Maqam track ${tr + 1}`"),
          X(EDITORS, "`sloop-fm6-bank-${today()}.syx`", "`maqam-fm6-bank-${today()}.syx`"),
          X(EDITORS, "`SLOOP-backup-${today()}.json`", "`Maqam-backup-${today()}.json`")]
    # ---- the audio preview's message, the manifest
    T += [X("webapp/editor/preview.js", "SLOOP preview requires audio at 44.1 kHz.", "Maqam preview requires audio at 44.1 kHz."),
          X("webapp/sloop.webmanifest", '"name": "SLOOP"', '"name": "Maqam"'),
          X("webapp/sloop.webmanifest", '"short_name": "SLOOP"', '"short_name": "Maqam"'),
          # the installed app opens the installer, never the hidden editor
          X("webapp/sloop.webmanifest", '"id": "editor/"', '"id": "installer/"'),
          X("webapp/sloop.webmanifest", '"start_url": "editor/"', '"start_url": "installer/"'),
          X("webapp/sloop.webmanifest", '"description": "SLOOP for the M-VAVE FM-1: the editor and the installer, also without internet"',
            '"description": "Maqam for the M-VAVE FM-1: quarter tones and 52 scales; the editor, the installer and an emulator, also without internet"')]
    # ---- the emulator page: its theme files sit in assets/ next to it when served alone (web/emu/build.sh); in the site they are one level up
    T += [X(EMU, re_(r'href="assets/(largen|largen\.components|theme-dark|maqam-theme|maqam)\.css"'), r'href="../assets/\1.css"', 5)]
    return T


def apply(site, entries):
    cache = {}
    for g, old, new, n in entries:
        files = sorted(site.glob(g))
        if not files and g == EMU:                     # (no emulator in this site)
            continue
        if not files:
            raise Fail(f"{g}: no such file in {site} (entry {old!r:.60})")
        for f in files:
            t = cache.get(f) or f.read_text(encoding="utf-8")
            c = len(old.findall(t)) if isinstance(old, re.Pattern) else t.count(old)
            if c != n:
                raise Fail(f"{f.relative_to(site)}: expected {n} x {old.pattern if isinstance(old, re.Pattern) else old!r:.90}, found {c}"
                           " (upstream changed its wording? update the table in tools/brand_site.py)")
            t = old.sub(new, t) if isinstance(old, re.Pattern) else t.replace(old, new)
            cache[f] = t
    for f, t in cache.items():
        f.write_text(t, encoding="utf-8")
    return len(entries)


# ----------------------------------------------------------------- the rest --- #
def rename_firmware(site):
    fw = site / "firmware"
    pk = [p for p in fw.glob("sloop-*.fwsc")]
    if len(pk) != 1:
        raise Fail(f"firmware/: expected one sloop-*.fwsc, found {len(pk)}")
    new = fw / ("maqam-" + pk[0].name[len("sloop-"):])
    pk[0].rename(new)
    sw = site / "webapp" / "sw.js"
    t = sw.read_text(encoding="utf-8")
    old = '"../firmware/' + pk[0].name + '"'
    if t.count(old) != 1:
        raise Fail(f"webapp/sw.js: expected {old} once in the file list")
    sw.write_text(t.replace(old, '"../firmware/' + new.name + '"'), encoding="utf-8")


def restamp(site):
    """sw.js's STAMP = version + a hash of the files it keeps (make_site.app_files); this pass changed them"""
    app = site / "webapp"
    sw = (app / "sw.js").read_text(encoding="utf-8")
    files = json.loads(re.search(r"const FILES = (\[.*?\]);", sw).group(1))
    h = hashlib.sha256()
    for k in sorted(files):
        p = (app / k).resolve()
        p = p / "index.html" if k.endswith("/") else p
        h.update(k.encode() + b"\0" + p.read_bytes())
    m = re.search(r'const STAMP = "(.*)-[0-9a-f]{12}"', sw)
    if not m:
        raise Fail("webapp/sw.js: no STAMP")
    (app / "sw.js").write_text(sw.replace(m.group(0), f'const STAMP = "{m.group(1)}-{h.hexdigest()[:12]}"'), encoding="utf-8")


def install_assets(site):
    """webapp/assets/: Largen, the Maqam theme, the installer's component file, the Rakkas font (all with their licences)"""
    a = site / "webapp" / "assets"
    a.mkdir(parents=True, exist_ok=True)
    for f in ("largen.css", "largen.components.css", "theme-dark.css", "LICENSE"):
        shutil.copy(VENDOR / f, a / (f if f != "LICENSE" else "LARGEN-LICENSE.txt"))
    for f in ("maqam-theme.css", "maqam.css", "installer.css"):
        shutil.copy(SITE_CSS / f, a / f)
    shutil.copy(FONT, a / "Rakkas-Regular.ttf")
    shutil.copy(FONT.parent / "OFL.txt", a / "RAKKAS-OFL.txt")


def add_to_sw(site):
    """the pages and their theme are kept for offline use too (make_site's list has the installer, the editor, the emulator)"""
    sw = site / "webapp" / "sw.js"
    t = sw.read_text(encoding="utf-8")
    m = re.search(r"const FILES = (\[.*?\]);", t)
    files = json.loads(m.group(1))
    for f in ("guide/", "scales/", "assets/largen.css", "assets/largen.components.css", "assets/theme-dark.css", "assets/maqam-theme.css", "assets/maqam.css",
              "assets/installer.css", "assets/Rakkas-Regular.ttf"):
        if f not in files:
            files.append(f)
    sw.write_text(t.replace(m.group(0), "const FILES = " + json.dumps(sorted(files)) + ";"), encoding="utf-8")


def main(site):
    site = Path(site)
    if not (site / "webapp" / "installer" / "index.html").exists():
        raise Fail(f"{site}: not a site made by web/make_site.py")
    png, w, h = wordmark()
    logo_img = IMG.format(src=data_uri(png), w=w, h=h)
    install_assets(site)
    n = apply(site, table(logo_img))
    for name, px, mk in (("icon-192.png", 192, False), ("icon-512.png", 512, False), ("icon-maskable-512.png", 512, True)):
        (site / "webapp" / name).write_bytes(icon(px, mk))
    meta = re.search(r'const meta = \{"version": "([^"]*)"', (site / "webapp" / "installer" / "index.html").read_text(encoding="utf-8"))
    P.main(site, meta.group(1) if meta else "?")                     # the landing page, the guide, the scales page
    rename_firmware(site)
    add_to_sw(site)
    restamp(site)
    print(f"brand_site: {site}: {n} replacements, wordmark {w}x{h}, icons, assets, firmware/maqam-*.fwsc")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    try:
        main(sys.argv[1])
    except Fail as e:
        print(f"brand_site: error: {e}", file=sys.stderr)
        sys.exit(1)
    except ImportError as e:
        print(f"brand_site: error: needs Pillow ({e})", file=sys.stderr)
        sys.exit(1)
