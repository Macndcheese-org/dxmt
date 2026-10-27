#!/bin/bash
# Releases of the MacNdCheese DXMT build.
#
#   prerelease.sh publish <version> <asset>    make (or replace) the pre-release <version> with <asset>
#   prerelease.sh promote                      make the pre-release a normal release if MacNdCheese
#                                              Wine has a normal release that it has not been paired with
#
# <version> is what the Metal HUD prints (DXMT_VERSION, i.e. `git describe --always`, for example
# v0.80-353-g01e8a0e), and it is also the tag.  A build publishes its version as a pre-release and
# deletes the previous pre-releases: only the latest build of the branch is kept.
#
# A pre-release becomes a normal release when Macndcheese-org/wine has a normal (not pre-release, not
# draft) release that no DXMT release has been paired with yet.  The pairing is written in the notes of
# the promoted release (<!-- mnc-wine-release: 11.20.0 -->), so a wine release promotes at most one
# DXMT build, and the builds that follow stay pre-releases until the next wine release.
#
# Needs gh, with GH_TOKEN, and GITHUB_REPOSITORY; "publish" also GITHUB_SHA.
# WINE_REPO overrides the repository whose releases are watched.
set -euo pipefail

repo=${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is not set}
wine_repo=${WINE_REPO:-Macndcheese-org/wine}
note_marker='<!-- mnc-prerelease-note -->'

# none, draft, prerelease or stable, for the release with this tag
release_state() {
  local out
  if ! out=$(gh api "repos/$repo/releases/tags/$1" --jq 'if .draft then "draft" elif .prerelease then "prerelease" else "stable" end' 2>&1); then
    case $out in
      *"HTTP 404"*) echo none; return ;;
      *) echo "cannot look up the release $1: $out" >&2; return 1 ;;
    esac
  fi
  echo "$out"
}

# tags of the pre-releases of this repository
prereleases() {
  gh api "repos/$repo/releases" --paginate --jq '.[] | select(.prerelease and (.draft | not)) | .tag_name'
}

publish() {
  local version=$1 asset=$2 state old
  [ -f "$asset" ] || { echo "$asset does not exist" >&2; return 1; }
  state=$(release_state "$version") || return 1
  case $state in
    stable) echo "$version is already a normal release; nothing to publish"; return 0 ;;
    none) ;;
    *) gh release delete "$version" --repo "$repo" --cleanup-tag --yes ;;   # the same commit, built again
  esac
  gh release create "$version" "$asset" --repo "$repo" \
    --target "${GITHUB_SHA:?GITHUB_SHA is not set}" \
    --prerelease \
    --title "DXMT $version" \
    --notes "Built from $GITHUB_SHA ($(git log -1 --format=%s)).

Version shown by the Metal HUD: \`$version\`

$note_marker
Pre-release.  It becomes a normal release by itself when MacNdCheese Wine has a normal release, and the next build of the branch replaces it before that."
  while read -r old; do
    [ -n "$old" ] && [ "$old" != "$version" ] || continue
    gh release delete "$old" --repo "$repo" --cleanup-tag --yes
  done < <(prereleases)
}

promote() {
  local newest marker pre notes
  newest=$(gh api "repos/$wine_repo/releases" --paginate \
    --jq '.[] | select((.draft | not) and (.prerelease | not)) | .tag_name' | sort -V | tail -1)
  if [ -z "$newest" ]; then
    echo "$wine_repo has no normal release yet"
    return 0
  fi
  marker="<!-- mnc-wine-release: $newest -->"
  if gh api "repos/$repo/releases" --paginate \
       --jq '.[] | select((.draft | not) and (.prerelease | not)) | .body' | grep -qF -- "$marker"; then
    echo "MacNdCheese Wine $newest already has its DXMT release"
    return 0
  fi
  pre=$(gh api "repos/$repo/releases" --paginate \
    --jq '.[] | select(.prerelease and (.draft | not)) | [.created_at, .tag_name] | @tsv' | sort | tail -1 | cut -f2)
  if [ -z "$pre" ]; then
    echo "MacNdCheese Wine $newest is out, but there is no DXMT pre-release to promote"
    return 0
  fi
  notes=$(gh release view "$pre" --repo "$repo" --json body --jq .body | sed "/^$note_marker\$/,\$d")
  gh release edit "$pre" --repo "$repo" --prerelease=false --latest --notes-file - <<EOF
$notes
Released with MacNdCheese Wine $newest.
$marker
EOF
  echo "$pre is now a normal release, paired with MacNdCheese Wine $newest"
}

case ${1:-} in
  publish) [ $# -eq 3 ] || { sed -n '2,5p' "$0"; exit 2; }; publish "$2" "$3" ;;
  promote) promote ;;
  *) sed -n '2,5p' "$0"; exit 2 ;;
esac
