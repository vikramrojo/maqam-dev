#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# maqam: the GitHub Pages site (pages.yml): main's site at the root, each open pull request's under pr/<number>/.
#   .github/scripts/pages_assemble.sh OUT_DIR      (GH_TOKEN, REPO = owner/name)
# A site is the maqam-site-* artifact of a successful firmware run (firmware.yml). Only runs of this repository are
# used: a pull request from a fork is never published (its pages would run on this site's origin). For a pull
# request: a run of its head commit, else the newest of its branch that still has a site (its page stays up while
# a new build runs). Writes OUT_DIR/../previews.txt: "number sha" of each pull request published.
set -eu
OUT=$1
LIST="$(dirname "$OUT")/previews.txt"
rm -rf "$OUT"
mkdir -p "$OUT"
: > "$LIST"

runs() {   # successful firmware runs of this repository matching the query (-f k=v ...), newest first
    gh api -X GET "repos/$REPO/actions/workflows/firmware.yml/runs" -f status=success -F per_page=20 "$@" \
        --jq ".workflow_runs[] | select(.head_repository.full_name == \"$REPO\") | .id"
}
site_of_run() {   # run id, directory: its site artifact unpacked there
    name=$(gh api "repos/$REPO/actions/runs/$1/artifacts" \
        --jq '.artifacts[] | select((.name | startswith("maqam-site-")) and (.expired | not)) | .name' | head -n 1)
    [ -n "$name" ] || return 1
    mkdir -p "$2"
    gh run download "$1" -R "$REPO" -n "$name" -D "$2"
}

found=
for id in $(runs -f branch=main -f event=push); do
    if site_of_run "$id" "$OUT"; then found=$id; break; fi
done
[ -n "$found" ] || { echo "pages: no site from a successful firmware run on main"; exit 1; }
echo "pages: / from run $found"

gh pr list -R "$REPO" --state open --limit 50 --json number,headRefOid,headRefName,isCrossRepository \
    --jq '.[] | select(.isCrossRepository | not) | "\(.number) \(.headRefOid) \(.headRefName)"' |
while read -r n sha ref; do
    done_pr=
    for id in $(runs -f head_sha="$sha") $(runs -f branch="$ref"); do
        if site_of_run "$id" "$OUT/pr/$n"; then done_pr=$id; break; fi
    done
    if [ -n "$done_pr" ]; then
        echo "pages: pr/$n/ from run $done_pr"
        echo "$n $sha" >> "$LIST"
    else
        echo "pages: pr/$n: no site yet"
    fi
done
