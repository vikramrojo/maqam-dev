#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# maqam: one comment on each published pull request with its preview's address (pages.yml), kept up to date.
#   .github/scripts/pages_comment.sh PREVIEWS_TXT PAGE_URL      (GH_TOKEN, REPO)
set -eu
MARK="<!-- maqam-preview -->"
while read -r n sha; do
    url="${2%/}/pr/$n/webapp/installer/"
    body="$MARK
**Preview of this pull request:** $url

The installer lists every firmware this pull request builds (the normal one, and test builds such as the pressure bend), built from \`$(echo "$sha" | cut -c1-7)\`. Not a release: back up first. The page goes away when the pull request is closed."
    id=$(gh api "repos/$REPO/issues/$n/comments" --paginate --jq ".[] | select(.body | startswith(\"$MARK\")) | .id" | head -n 1)
    if [ -n "$id" ]; then
        gh api -X PATCH "repos/$REPO/issues/comments/$id" -f body="$body" >/dev/null
    else
        gh api -X POST "repos/$REPO/issues/$n/comments" -f body="$body" >/dev/null
    fi
done < "$1"
