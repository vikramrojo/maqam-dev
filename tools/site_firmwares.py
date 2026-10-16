#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""maqam: put more than one firmware package on a site's installer (a preview of a branch or a pull request).

  tools/site_firmwares.py SITE --title TEXT --base-label TEXT ID LABEL PACKAGE VERSION [ID LABEL PACKAGE VERSION ...]

Run after web/make_site.py and tools/brand_site.py. The site already installs its own package (the build's
normal firmware, id "maqam"); each extra package is copied to firmware/maqam-VERSION.fwsc (checked as
make_site checks it: a Felucca package with the update loader) and listed on the installer:

  * the installer's `const meta = {...};` becomes the chosen entry of a list, by the page's ?fw=ID (the first,
    the site's own package, without it), so the page's script, its download check (sha256) and its install
    are unchanged;
  * a chooser above the Install button: TEXT (what this preview is), a link per package, the chosen one marked.

Like brand_site.py, nothing in the upstream web sources is edited; a page whose wording changed fails the run.
"""
import hashlib
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "web"))
import brand_site as B  # noqa: E402
import make_site as M  # noqa: E402

INSTALLER = Path("webapp") / "installer" / "index.html"
META = re.compile(r"const meta = (\{[^\n]*?\});")
GO = '<p><button id="go"'


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def main(argv):
    if len(argv) < 5 or argv[1] != "--title" or argv[3] != "--base-label" or (len(argv) - 5) % 4:
        sys.exit(__doc__)
    site, title, base_label, rest = Path(argv[0]), argv[2], argv[4], argv[5:]
    page = site / INSTALLER
    html = page.read_text(encoding="utf-8")
    m = META.findall(html)
    if len(m) != 1 or html.count(GO) != 1:
        raise B.Fail(f"{INSTALLER}: expected one `const meta = {{...}};` and one {GO!r} (made by make_site + brand_site?)")
    entries = [dict(id="maqam", label=base_label, **json.loads(m[0]))]
    for i in range(0, len(rest), 4):
        fid, label, pkg, version = rest[i:i + 4]
        if not re.fullmatch(r"[a-z0-9-]{1,16}", fid) or any(e["id"] == fid for e in entries):
            raise B.Fail(f"{fid!r}: an id is 1..16 of a-z 0-9 -, once")
        raw = Path(pkg).read_bytes()
        product = M.product_of(raw)
        if not re.fullmatch(r"FM-1_9\d\d", product) or b"FELUCCA-LOADER-1" not in raw:
            raise B.Fail(f"{pkg}: not a Felucca package with its update loader ({product!r})")
        name = "maqam-" + re.sub(r"[^A-Za-z0-9.-]", "-", version) + ".fwsc"
        (site / "firmware" / name).write_bytes(raw)
        entries.append(dict(id=fid, label=label, version=version, product=product, pkg="../../firmware/" + name,
                            sha256=hashlib.sha256(raw).hexdigest()))
    pick = ("((all) => { const q = new URLSearchParams(location.search).get(\"fw\"); "
            "return all.find((m) => m.id === q) || all[0]; })(" + json.dumps(entries) + ")")
    html = META.sub(lambda _: "const meta = " + pick + ";", html)
    items = "".join(f'<li><a href="?fw={e["id"]}" data-fw="{e["id"]}">{esc(e["label"])}</a> '
                    f'<span class="small">{esc(e["version"])}</span></li>' for e in entries)
    chooser = (f'<section class="firmwares stack" aria-label="Firmware in this build">\n'
               f'    <p class="small"><b>{esc(title)}</b> Choose the firmware to install:</p>\n'
               f'    <ul>{items}</ul>\n'
               '    <script>{ const q = new URLSearchParams(location.search).get("fw"); const a = document.querySelectorAll(".firmwares a");'
               ' const cur = [...a].find((x) => x.dataset.fw === q) || a[0]; cur.setAttribute("aria-current", "true");'
               ' cur.insertAdjacentHTML("afterend", " <b>(chosen)</b>"); }</script>\n'
               '  </section>\n  ')
    page.write_text(html.replace(GO, chooser + GO), encoding="utf-8")
    B.restamp(site)
    print(f"site_firmwares: {site}: {len(entries)} packages: " + ", ".join(e["id"] for e in entries))


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except B.Fail as e:
        print(f"site_firmwares: error: {e}", file=sys.stderr)
        sys.exit(1)
