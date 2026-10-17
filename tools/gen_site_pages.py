#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""maqam: the site's two explanatory pages, written into a site made by web/make_site.py:

  webapp/guide/    "How to play Maqam": web/site/getting-started.md + SCALES.md (the single source of truth for how
                   the scale screens work), converted by the small Markdown converter below, with the screenshots
                   of assets/screens/scales/ copied next to it
  webapp/scales/   "The 73 scales and their temperament": generated from scales/*.txt through tools/gen_scales.py's
                   own parser, so the page can not drift from the firmware's table

  tools/gen_site_pages.py SITE_DIR      (tools/brand_site.py calls it)
Standard library only. No CDN, no script. One theme: web/site/maqam.css on Largen (see tools/brand_site.py for the assets).
"""
import html
import math
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import gen_scales as G  # noqa: E402

REPO = "https://github.com/vikramrojo/maqam-dev"
NOTE24 = ["C", "C+", "C#", "Dd", "D", "D+", "D#", "Ed", "E", "Fd", "F", "F+", "F#", "Gd", "G", "G+", "G#", "Ad", "A", "A+", "A#", "Bd", "B", "Cd"]


# ----------------------------------------------------------- Markdown -> HTML --- #
def inline(s):
    codes = []
    s = re.sub(r"`([^`]+)`", lambda m: codes.append(m.group(1)) or f"\0{len(codes) - 1}\0", s)
    s = html.escape(s, quote=False)
    s = re.sub(r"!\[([^\]]*)\]\(([^)\s]+)\)", lambda m: f'<img src="{html.escape(m.group(2), quote=True)}" alt="{html.escape(m.group(1), quote=True)}" loading="lazy">', s)
    s = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", lambda m: f'<a href="{html.escape(m.group(2), quote=True)}">{m.group(1)}</a>', s)
    s = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", s)
    s = re.sub(r"(?<![\w*])\*([^*\s][^*]*?)\*(?![\w*])", r"<em>\1</em>", s)
    return re.sub(r"\0(\d+)\0", lambda m: "<code>" + html.escape(codes[int(m.group(1))], quote=False) + "</code>", s)


LI = re.compile(r"^(\s*)([-*]|\d+\.)\s+(.*)$")
TSEP = re.compile(r"^\s*\|?[\s:|-]+\|[\s:|-]*$")


def cells(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [c.strip() for c in line.split("|")]


def md_to_html(text):
    L = text.replace("\r\n", "\n").split("\n")
    out = []
    i = 0

    def is_block(k):
        s = L[k]
        return (not s.strip() or s.startswith("```") or re.match(r"^#{1,6}\s", s) or re.match(r"^-{3,}\s*$", s)
                or LI.match(s) or (s.lstrip().startswith("|") and k + 1 < len(L) and TSEP.match(L[k + 1])))

    def parse_list(k, base):
        ordered = bool(re.match(r"\d", LI.match(L[k]).group(2)))
        tag = "ol" if ordered else "ul"
        h = [f"<{tag}>"]
        while k < len(L):
            m = LI.match(L[k])
            if not m or len(m.group(1)) != base:
                break
            body = [m.group(3)]
            k += 1
            sub = ""
            while k < len(L) and L[k].strip():
                n = LI.match(L[k])
                if n and len(n.group(1)) > base:
                    sub, k = parse_list(k, len(n.group(1)))
                elif n:
                    break
                else:
                    body.append(L[k].strip())
                    k += 1
            h.append("<li>" + inline(" ".join(body)) + sub + "</li>")
        h.append(f"</{tag}>")
        return "".join(h), k

    while i < len(L):
        s = L[i]
        if not s.strip():
            i += 1
        elif s.startswith("```"):
            j = i + 1
            while j < len(L) and not L[j].startswith("```"):
                j += 1
            out.append("<pre>" + html.escape("\n".join(L[i + 1:j]), quote=False) + "</pre>")
            i = j + 1
        elif re.match(r"^#{1,6}\s", s):
            n = len(s) - len(s.lstrip("#"))
            t = s[n:].strip()
            slug = re.sub(r"[^a-z0-9]+", "-", re.sub(r"<[^>]+>", "", inline(t)).lower()).strip("-")
            out.append(f'<h{n} id="{slug}">{inline(t)}</h{n}>')
            i += 1
        elif re.match(r"^-{3,}\s*$", s):
            out.append("<hr>")
            i += 1
        elif s.lstrip().startswith("|") and i + 1 < len(L) and TSEP.match(L[i + 1]):
            head = cells(s)
            j = i + 2
            rows = []
            while j < len(L) and L[j].lstrip().startswith("|"):
                rows.append(cells(L[j]))
                j += 1
            hd = "".join(f"<th>{inline(c)}</th>" for c in head) if any(head) else ""
            out.append('<div class="table-wrap"><table>' + (f"<thead><tr>{hd}</tr></thead>" if hd else "") + "<tbody>"
                       + "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>" for r in rows) + "</tbody></table></div>")
            i = j
        elif LI.match(s):
            h, i = parse_list(i, len(LI.match(s).group(1)))
            out.append(h)
        else:
            j = i
            para = []
            while j < len(L) and (j == i or not is_block(j)):
                para.append(L[j].strip())
                j += 1
            out.append("<p>" + inline(" ".join(para)) + "</p>")
            i = j
    return "\n".join(out)


# --------------------------------------------------------------------- shell --- #
# one theme (web/site/maqam.css on Largen) and one header / nav / footer for every page of the site
SOURCE = REPO
NAV = [("Home", None), ("Install", "installer/"), ("Try it", "emu/"), ("How to play", "guide/"), ("The 73 scales", "scales/"), ("Source", SOURCE)]
ASSET_FILES = ["largen.css", "largen.components.css", "theme-dark.css", "maqam-theme.css", "maqam.css"]


def head_links(w, installer=False):
    """the stylesheet links; w = the path from the page to webapp/ ('' for webapp/x pages use '../')"""
    fl = ASSET_FILES + (["installer.css"] if installer else [])
    return "\n".join(f'<link rel="stylesheet" href="{w}assets/{f}">' for f in fl)


def site_header(root, webapp, current=None):
    """root: the path to the site root ('' on the landing page, '../../' on webapp/x/); webapp: to webapp/"""
    items = []
    for name, dest in NAV:
        if dest is None:
            href = root or "./"
        elif dest.startswith("http"):
            href = dest
        else:
            href = (webapp + dest) if root else ("webapp/" + dest)
        cur = ' aria-current="page"' if name == current else ""
        items.append(f'<a href="{href}"{cur}>{name}</a>')
    return ('<header class="site-header center"><a class="wordmark" href="' + (root or "./") + '" title="Maqam">Maqam</a>'
            '<nav class="site-nav" aria-label="Site">' + "".join(items) + "</nav></header>\n"
            '<div class="rule-wrap center"><div class="rule" aria-hidden="true">'
            '<i data-tone="success"></i><i data-tone="danger"></i><i data-tone="warning"></i><i data-tone="secondary"></i></div></div>')


def site_footer(root):
    lic = REPO + "/blob/main/LICENSING.md"
    return ('<footer class="site-footer center">'
            f'<p>Maqam &middot; free software (GPL-3.0) &middot; <a href="{REPO}">Source</a> &middot; <a href="{lic}">Licences and credits</a> &middot; '
            'styled with <a href="https://largen.dev">Largen</a> (MIT)</p>'
            '<p>Based on <a href="https://github.com/isod89/sloop-fm1">SLOOP</a> by isod89 and <a href="https://github.com/hugelton/Felucca">Felucca</a> '
            'by Leo Kuroshita, H&uuml;gelton Instruments. M-VAVE and FM-1 are trademarks of their respective owners.</p></footer>')


def shell(title, body, current=None, root="../../", webapp="../", head_extra=""):
    manifest = f'{webapp if root else "webapp/"}sloop.webmanifest'
    w = webapp if root else "webapp/"
    return f"""<!doctype html>
<html lang="en" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<meta name="theme-color" content="#08090b">
<link rel="manifest" href="{manifest}"><link rel="icon" href="{w}icon-192.png">
{head_links(w)}
{head_extra}</head>
<body>
{site_header(root, webapp, current)}
<main class="page center stack">
{body}
</main>
{site_footer(root)}
</body>
</html>
"""


# --------------------------------------------------------------------- guide --- #
def guide(site):
    out = site / "webapp" / "guide"
    (out / "img").mkdir(parents=True, exist_ok=True)
    md = (ROOT / "SCALES.md").read_text(encoding="utf-8")
    start = (ROOT / "web" / "site" / "getting-started.md").read_text(encoding="utf-8")
    # the guide is Maqam's: the base firmware's name is for the compatibility section of the home page only
    for a, b in ((r"SLOOP 2\.6 writes labels", "Maqam writes labels"), (r"as in SLOOP 2\.6", "as in the base firmware"),
                 (r"any other SLOOP 2\.6 parameter lock", "any other parameter lock"), (r"\s*The web editor lists the CENT lock in the step's lock list\.", ""), (r"\bSLOOP\b", "Maqam")):
        md = re.sub(a, b, md)
    imgs = sorted(set(re.findall(r"\]\((assets/screens/scales/[^)\s]+\.png)\)", md)))
    for p in imgs:
        shutil.copy(ROOT / p, out / "img" / Path(p).name)
    md = re.sub(r"\]\(assets/screens/scales/([^)\s]+)\)", r"](img/\1)", md)
    md = re.sub(r"\]\((?!https?:|#|img/|\.\./)([^)\s]+)\)", lambda m: f"]({REPO}/blob/main/{m.group(1)})", md)
    md = md.replace("# The scale screens: a visual guide", "# How to play Maqam: the scale screens", 1)
    body = '<div class="prose">' + md_to_html(start) + md_to_html(md) + "</div>"
    (out / "index.html").write_text(shell("How to play Maqam", body, "How to play"), encoding="utf-8")
    return len(imgs)


# -------------------------------------------------------------------- scales --- #
SETS = [
    ("WEST", "Western", "12-tone equal temperament",
     "Every semitone is exactly 100 cents, so every offset is 0: the grid and the traditional tuning are the same. "
     "These are the 16 scales the firmware already had (major, the modes, pentatonics, blues, whole-tone, diminished)."),
    ("TURK", "Turkish makam", "Arel-Ezgi-Uzdilek system, Pythagorean ratios",
     "The Arel-Ezgi-Uzdilek theory divides the whole tone into 9 commas (a 53-comma octave, one comma = 1200/53 = 22.64 cents). "
     "The steps have names: <i>bakiye</i> 4 commas (256/243), <i>k&uuml;&ccedil;&uuml;k m&uuml;cennep</i> 5 (2187/2048), "
     "<i>b&uuml;y&uuml;k m&uuml;cennep</i> 8 (65536/59049) and <i>tanini</i> 9 (9/8). Each degree is written as its number of commas "
     "(n &times; 1200/53 cents); the source file lists the comma steps of every makam."),
    ("ARAB", "Arabic maqam", "al-&Scaron;abb&amacr;gh (1954), 24-tone ratios",
     "The 24-tone ratios published by al-&Scaron;abb&amacr;gh in 1954: <i>d&uacute;g&amacr;h</i> 9/8, "
     "<i>seg&amacr;h</i> 102/83, <i>chah&amacr;rg&amacr;h</i> 4/3, <i>naw&amacr;</i> 3/2, <i>&#7717;usayn&imacr;</i> 27/16, "
     "<i>awj</i> 400/217, <i>&#703;ajam</i> 883/490, <i>kurd&imacr;</i> 6/5 and the rest, taken from r&amacr;st; a maqam on another note "
     "is measured from its own tonic. These are theoretical values: players and tuning traditions differ by a few cents."),
    ("INDIA", "Indian raga", "just intonation, the swaras",
     "The twelve swaras in just intonation: S 1/1, r 16/15, R 9/8, g 6/5, G 5/4, m 4/3, M 45/32, P 3/2, d 8/5, D 5/3, n 16/9, "
     "N 15/8 (lower case komal, M tivra). The raga lists give the ascending (aroha) and descending (avaroha) notes; Sa is the root."),
    ("PSYCH", "Microtonal psych", "24-tone equal temperament, after King Gizzard and the Lizard Wizard",
     "Makam and maqam scales as quarter-tone-fretted guitars play them: every note on the 50-cent grid, so every offset is 0. "
     "Four families (4 + 3 + 3 + 3), each a 12-tone frame with two notes that move: <i>H&uuml;seyni</i> (the 2nd and 6th: neutral, minor "
     "or major), <i>Hijaz</i> (the 3rd and 7th narrowed to neutral), <i>subminor Phrygian</i> (the 2nd and 6th a quarter tone above "
     "the note below) and <i>Bhairav</i> (raga Bhairav's re and dha, a quarter tone low, or neutral as in Persian <i>Chahargah</i>), and <i>Rast</i> (its neutral 3rd, with a neutral, minor or major 7th). After The Pop Descriptivist's "
     "<a href=\"https://www.youtube.com/watch?v=TiGI-5i2wZ8\">The Microtonal Scales of King Gizzard and the Lizard Wizard</a>."),
    ("JINS", "Jins combinations", "24-tone equal temperament, two different jinsas",
     "A lower jins from one PSYCH family and an upper jins, on the 5th, from another: the way King Gizzard's "
     "<i>K.G.L.W.</i> scale joins a H&uuml;seyni pentachord to a neutral Hijaz tetrachord. Every row is attested: that song, "
     "the Arabic maqamat <i>Nairuz</i> (Rast + Bayati), <i>Suznak</i> (Rast + Hijaz) and <i>Hijaz</i> with Rast on the 4th, and "
     "raga <i>Darbari Kanada</i>, whose ga and dha are both sung low (Kanada + subminor). Every note is on the 50-cent grid."),
]


def note_html(name):
    if len(name) == 2 and name[1] in "d+":
        return f'{name[0]}<sup class="q">{name[1]}</sup>'
    return html.escape(name)


def degrees(sc, tune, mask_form):
    """[(slot, exact cents)] of one form, ascending in pitch"""
    frm = G.cents(sc["from"][0]) if sc["from"] else 0.0
    seen = {}
    for tok in sc[mask_form] if sc[mask_form] is not None else sc["up"]:
        c = (G.cents(tok) - frm) % 1200.0
        if c > 1199.9999:
            c = 0.0
        slot = G.rnd(c / 50.0) % 24
        seen[slot] = c
    return sorted(((s, c) for s, c in seen.items()), key=lambda x: x[1])


def scales_data(scales_dir=None):
    d = Path(scales_dir) if scales_dir else ROOT / "scales"
    res = G.load(d)                                    # the checks of the build, and the masks / tunes
    raw = {}
    for s in G.SETS:
        for sc in G.parse_file(d / G.FILES[s], s):
            raw[sc["code"]] = sc
    rows = []
    for r in res:
        sc = raw[r["code"]]
        up = degrees(sc, r["tune"], "up")
        dn = degrees(sc, r["tune"], "down") if r["asc"] != r["desc"] else None
        rows.append(dict(r, up=up, down=dn))
    return rows


def form_cells(form, tune):
    """a degree: note name on C, EQUAL cents, TRADITIONAL cents (exact)"""
    h = []
    for slot, c in form:
        h.append(f'<td class="deg"><b>{note_html(NOTE24[slot])}</b><br><span class="eq" data-tone="primary">{50 * slot}</span>'
                 f'<br><span class="tr" data-tone="danger">{c:.1f}</span></td>')
    return "".join(h)


DOWN = '<span class="dir" data-tone="warning">down</span>'      # the descending row: the word in the tone's colour, no pill


def scales_page(site, rows=None):
    rows = rows or scales_data()
    maxdeg = max(len(r["up"]) for r in rows)
    parts = ["<h1>The 73 scales and their temperament</h1>",
             "<p class=\"lead\">Maqam has 73 scales in six sets. This page is generated from the data file the firmware is built from, "
             "so what it lists is what the FM-1 plays.</p>",
             "<p>On the FM-1 the scale is chosen by holding <b>SEL</b>: <b>PRESETS</b> picks the set, <b>KNOB 2</b> the scale "
             "(<a href=\"../guide/\">how to play</a>).</p>",
             "<section class=\"stack\" id=\"how\" style=\"--gap: var(--space-3)\"><h2>EQUAL, TRADITIONAL, and any root</h2>"
             "<ul class=\"stack\" style=\"--gap: var(--space-2); padding-inline-start: 1.2em\"><li><b>EQUAL</b> puts every note on the 24-step quarter-tone grid: 50 cents a step, so a note a "
             "quarter tone below E is exactly halfway between E-flat and E.</li>"
             "<li><b>TRADITIONAL</b> (the default) nudges each degree by the offset listed here, to the pitch of the tradition's own "
             "theory: at most &plusmn;25 cents from the grid. The two lines of cents under each note are exactly that: "
             "<span class=\"eq\" data-tone=\"primary\">EQUAL</span> and <span class=\"tr\" data-tone=\"danger\">TRADITIONAL</span>. WEST scales have no offsets.</li>"
             "<li><b>Any scale plays on any root</b> (24 of them, half-flats included), with the same intervals and tuning. "
             "<i>Home</i> is the scale's traditional tonic, shown on the FM-1 as a hint; the notes below are spelled on C.</li>"
             "<li><b>Up and down.</b> Some scales differ going down: they get a second row marked " + DOWN + ". A key lower than the previous key "
             "uses the descending form, any other key the ascending one. A scale with fewer notes than keys repeats a note on neighbouring keys.</li></ul>"
             "<p class=\"small\">Notes: <b>d</b> = half-flat (a quarter tone below), <b>+</b> = half-sharp (a quarter tone above): "
             "<b>E<sup class=\"q\">d</sup></b> is E half-flat, <b>F<sup class=\"q\">+</sup></b> is F half-sharp. Each degree shows its note, then its cents above the root "
             "in EQUAL and in TRADITIONAL tuning. Tables scroll sideways on a narrow screen.</p></section>"]
    for si, (code, name, temper, intro) in enumerate(SETS):
        sub = [r for r in rows if r["set"] == si]
        parts.append(f'<section class="stack" id="{code.lower()}" style="--gap: var(--space-3)"><h2>{name}: {temper} <span class="small">({len(sub)} scales)</span></h2><p>{intro}</p>')
        t = ['<div class="table-wrap"><table class="scales"><thead><tr><th>Code</th><th>Name</th><th>Home</th><th></th>'
             f'<th colspan="{maxdeg}">Degrees: note &middot; EQUAL &middot; TRADITIONAL (cents)</th></tr></thead><tbody>']
        for r in sub:
            n = len(r["up"])
            t.append(f'<tr class="up"><th scope="row"><code>{html.escape(r["code"])}</code></th><td>{html.escape(r["name"])}</td>'
                     f'<td>{note_html(NOTE24[r["home"]])}</td><td class="small">up</td>{form_cells(r["up"], r["tune"])}'
                     + f'<td colspan="{maxdeg - n}"></td>' * (maxdeg > n) + "</tr>")
            if r["down"]:
                m = len(r["down"])
                t.append(f'<tr class="down"><td></td><td></td><td></td><td class="small">{DOWN}</td>{form_cells(r["down"], r["tune"])}'
                         + f'<td colspan="{maxdeg - m}"></td>' * (maxdeg > m) + "</tr>")
        t.append("</tbody></table></div></section>")
        parts.append("".join(t))
    parts.append('<p class="small">Source of the numbers: <a href="' + REPO + '/tree/main/scales">scales/*.txt</a> '
                 '(Scala-style cents and ratios, with the comma steps and the ratio tables in their comments).</p>')
    out = site / "webapp" / "scales"
    out.mkdir(parents=True, exist_ok=True)
    (out / "index.html").write_text(shell("The 73 scales and their temperament", "\n".join(parts), "The 73 scales"), encoding="utf-8")
    return len(rows)


def landing(site, version):
    """the site's root page: tools/brand_site_index.html is its body"""
    body = (HERE / "brand_site_index.html").read_text(encoding="utf-8").replace("{{VERSION}}", version)
    if not (site / "webapp" / "emu" / "index.html").exists():        # no emulator in this site
        body, k = re.subn(r'\s*<a class="link-card" data-tone="success" href="webapp/emu/".*?</a>', "", body, flags=re.S)
        if k != 1:
            raise SystemExit("tools/brand_site_index.html: the emulator card is gone")
    (site / "index.html").write_text(shell("Maqam: quarter tones for the M-VAVE FM-1", body, "Home", root="", webapp=""), encoding="utf-8")


def main(site, version="?"):
    site = Path(site)
    n_img = guide(site)
    n_sc = scales_page(site)
    landing(site, version)
    print(f"gen_site_pages: {site}: landing, guide ({n_img} images), scales page ({n_sc} scales)")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "?")
