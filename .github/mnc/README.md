# MacNdCheese automation

| File | What it does |
| --- | --- |
| `workflows/build-release.yml` | Only on `mncDXMT`, which is where `scripts/build-all.sh` lives: builds the DLLs (d3d9 included) on every push and publishes them as a pre-release named after the Metal HUD version (`git describe --always`). |
| `workflows/promote-release.yml` | Every hour: if `Macndcheese-org/wine` has a normal release that no DXMT release has been paired with, the DXMT pre-release becomes a normal release. |
| `workflows/sync-upstream.yml` | Every three hours: merges `3Shain/dxmt` `main` into `main` and `mncDXMT` when they are behind, and pushes (never forced). Starts the build after a push to `mncDXMT`. |
| `mnc/prerelease.sh`, `mnc/sync-upstream.sh` | The logic of the three workflows; both read their own header. |

Scheduled workflows only run from the default branch, so `sync-upstream.yml` and `promote-release.yml` have to be on `main`.

## When the sync cannot push

A merge that conflicts is not pushed: an issue (label `upstream-sync`) lists the files and the commands to merge by hand. The same issue is opened when the merge is clean but changes `.github/workflows`, because the `GITHUB_TOKEN` cannot push workflow changes. To let the sync do those too, add a `SYNC_TOKEN` secret (fine-grained token on this repository, `Contents` and `Workflows` read and write). With it the push also starts the push workflows (CI Build on `main`).

The issue closes itself once the branch is up to date.

## Keeping a release

Only the newest pre-release is kept: the next build deletes it. A normal release is never touched.
