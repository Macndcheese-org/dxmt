#!/bin/bash
# Bring a branch of this fork up to date with the upstream DXMT (3Shain/dxmt, branch main).
#
#   sync-upstream.sh <branch>     run in a checkout of <branch> that has its whole history
#
# It does what the "Merge branch '3Shain:main' into <branch>" commits do: merge upstream/main into the
# branch (a fast-forward when the branch has nothing of its own), and push it.  The push is never
# forced.  When the merge has conflicts, nothing is pushed: the merge is dropped and an issue
# lists the conflicting files and how to merge by hand.  The same goes for a token that is not allowed
# to push changes to .github/workflows (the GITHUB_TOKEN never is; a token in the SYNC_TOKEN secret
# can be).  A branch that moved while the job ran is left for the next run.
#
# Needs git, gh with GH_TOKEN, and GITHUB_REPOSITORY.
#   UPSTREAM_URL     where upstream is fetched from (default https://github.com/3Shain/dxmt)
#   UPSTREAM_BRANCH  its branch (default main)
#   BUILD_BRANCH     after a push to this branch, its build is started (default mncDXMT)
#   DRY_RUN          if set, merge but push nothing and write nothing on GitHub
set -euo pipefail

branch=${1:?usage: sync-upstream.sh <branch>}
repo=${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is not set}
upstream_url=${UPSTREAM_URL:-https://github.com/3Shain/dxmt}
upstream_branch=${UPSTREAM_BRANCH:-main}
build_branch=${BUILD_BRANCH:-mncDXMT}
title="sync: $branch cannot be brought up to date with upstream automatically"
label="upstream-sync"

gh_() { if [ -n "${DRY_RUN:-}" ]; then echo "[dry run] gh $*"; else gh "$@"; fi; }

# a fork starts with its issues disabled
issues_enabled() { [ "$(gh api "repos/$repo" --jq .has_issues)" = true ]; }

open_issue() {
  issues_enabled || return 0
  gh api "repos/$repo/issues?labels=$label&state=open&per_page=100" \
    --jq ".[] | select(.title == \"$title\") | .number" | head -1
}

# set when something needs a person and could not be put in an issue: the job then ends red
attention=0

# report <upstream sha> <what happened>: one open issue per branch, updated when upstream moves on
report() {
  local sha=$1 body=$2 number
  body="$body

<!-- upstream-sha: $sha -->"
  [ -z "${DRY_RUN:-}" ] || { echo "[dry run] issue: $title"; echo "$body"; return 0; }
  if ! issues_enabled; then
    echo "::error::$title (issues are disabled on $repo: enable them in Settings > General > Features to get an issue)"
    printf '## %s\n\n%s\n' "$title" "$body" >> "${GITHUB_STEP_SUMMARY:-/dev/null}"
    attention=1
    return 0
  fi
  number=$(open_issue)
  if [ -z "$number" ]; then
    gh label create "$label" --repo "$repo" --color FBCA04 --force
    gh issue create --repo "$repo" --title "$title" --label "$label" --body "$body"
  elif ! gh issue view "$number" --repo "$repo" --json body --jq .body | grep -qF "<!-- upstream-sha: $sha -->"; then
    gh issue edit "$number" --repo "$repo" --body "$body"
    gh issue comment "$number" --repo "$repo" --body "Upstream moved on to ${sha:0:9}; this is still the case."
  fi
}

# the branch is up to date: close the issue that was about it
resolved() {
  local number
  [ -z "${DRY_RUN:-}" ] || return 0
  number=$(open_issue)
  [ -z "$number" ] || gh issue close "$number" --repo "$repo" --comment "$branch is up to date with upstream."
}

git config user.name "github-actions[bot]"
git config user.email "41898282+github-actions[bot]@users.noreply.github.com"
git remote remove upstream 2>/dev/null || true
git remote add upstream "$upstream_url"
git fetch --quiet --no-tags upstream "+refs/heads/$upstream_branch:refs/remotes/upstream/$upstream_branch"

upstream=upstream/$upstream_branch
sha=$(git rev-parse "$upstream")
behind=$(git rev-list --count "HEAD..$upstream")
if [ "$behind" -eq 0 ]; then
  echo "$branch is up to date with $upstream_url ($upstream_branch)"
  resolved
  exit 0
fi
echo "$branch is $behind commit(s) behind $upstream_url ($upstream_branch)"

if ! git merge --no-edit -m "Merge branch '3Shain:$upstream_branch' into $branch" "$upstream"; then
  files=$(git diff --name-only --diff-filter=U | sed 's/^/- `/; s/$/`/')
  git merge --abort
  report "$sha" "\`$branch\` is $behind commit(s) behind [$upstream_url]($upstream_url) and merging \`$upstream_branch\` conflicts in:

$files

Nothing was pushed.  To merge by hand:

\`\`\`sh
git fetch origin $branch
git fetch $upstream_url $upstream_branch
git switch -c sync-$branch origin/$branch
git merge FETCH_HEAD   # resolve the conflicts, then commit
git push origin sync-$branch:$branch
\`\`\`

This issue closes itself once \`$branch\` is up to date."
  echo "::warning::$branch: merging $upstream conflicts, see the issue"
  exit "$attention"
fi

if [ -n "${DRY_RUN:-}" ]; then
  echo "[dry run] merged $upstream into $branch, not pushing"
  exit 0
fi

if ! out=$(git push origin "HEAD:refs/heads/$branch" 2>&1); then
  echo "$out"
  case $out in
    *workflow*)
      report "$sha" "\`$branch\` is $behind commit(s) behind [$upstream_url]($upstream_url) and merges cleanly, but the merge changes \`.github/workflows\`, which the token of the sync job is not allowed to push.

Merge it by hand (see the commands in the README of \`.github/mnc\`), or add a \`SYNC_TOKEN\` secret: a fine-grained token on this repository with the \`Contents\` and \`Workflows\` read and write permissions.

This issue closes itself once \`$branch\` is up to date."
      echo "::warning::$branch: the token cannot push workflow changes, see the issue"
      exit "$attention" ;;
    *rejected*|*"fetch first"*)
      echo "::warning::$branch moved while it was merged; the next run tries again"
      exit 0 ;;
    *) exit 1 ;;
  esac
fi
resolved

if [ "$branch" = "$build_branch" ]; then
  # a push made with the GITHUB_TOKEN does not start the workflows that listen to push
  gh workflow run build-release.yml --repo "$repo" --ref "$build_branch" ||
    echo "::warning::the build of $build_branch could not be started"
fi
