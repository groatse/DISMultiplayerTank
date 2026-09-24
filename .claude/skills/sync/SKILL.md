---
name: sync
description: Sync this Unreal project with the GitHub remote — pull, commit, push in one step. ONLY invoke when the user explicitly types /sync or directly asks to sync. Never invoke proactively.
---

# Sync the project with GitHub

This is a solo project that moves between machines. `.uasset`/`.umap` files are binary and
**cannot be merged** — a conflict on one is not resolvable, only avoidable. So this skill
always pulls before it pushes, and stops rather than manufacture an unmergeable conflict.

## Steps

### 1. Check the editor isn't holding files open

If the Unreal editor is running, assets may be half-written or unsaved.

```powershell
Get-Process UnrealEditor -EA SilentlyContinue
```

If it's running, warn the user and ask them to save all (`Ctrl+Shift+S`) and ideally close
the editor before continuing. Do not proceed silently.

### 2. Pull first, always

```powershell
git fetch origin
git status -sb
```

- If local and remote have **both** moved (diverged), **stop**. Do not rebase or merge
  automatically — if the divergence touches any `.uasset`/`.umap`, one side's work will be
  lost. Show the user what changed on each side and let them choose.
- Otherwise, if the remote is ahead: `git pull --rebase`.

### 3. Stage and commit

```powershell
git add -A
git status --short
```

Review what changed and write a real commit message describing the work — group it by what
actually moved (C++ source vs. Blueprint/content assets vs. config). Never use a filler
message like "update" or "sync".

Then commit. If nothing changed, say so and skip to step 5.

### 4. Push

```powershell
git push
```

Content pushes move LFS objects and can take a while on a slow link — that's expected.

### 5. Report LFS headroom

GitHub Free includes **10 GiB** of LFS storage, and LFS keeps *every revision of every
binary asset forever* — so usage grows with asset churn, not with project size. Re-saving one
27 MB texture fifty times costs 1.35 GiB, permanently.

Do **not** try to read this from the GitHub API. The old
`/user/settings/billing/shared-storage` endpoint was retired in the move to metered billing
and every replacement 404s for personal accounts. Compute it from the local repo instead —
`--all` walks every ref, so this counts historical revisions, not just the current checkout:

```powershell
$all = git lfs ls-files --all --size
$bytes = 0
$all | ForEach-Object {
  if ($_ -match '\(([\d.]+)\s*(B|KB|MB|GB)\)\s*$') {
    $v = [double]$Matches[1]
    switch ($Matches[2]) { 'B' {$m=1} 'KB' {$m=1KB} 'MB' {$m=1MB} 'GB' {$m=1GB} }
    $bytes += $v * $m
  }
}
"{0:N2} GiB of 10 GiB ({1:N1}%)" -f ($bytes/1GB), (100*$bytes/10GB)
```

Report the figure. Past ~7 GiB, tell the user the escape hatch: move the remote to Azure
DevOps, which has unlimited free LFS and takes identical git commands — only the remote URL
changes.

## Reminders for the user

- `/sync` when you **sit down** at a machine and again before you **walk away**. The whole
  scheme depends on never editing the same asset on two machines without pushing in between.
- Build output (`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`) is deliberately
  not tracked. On a fresh clone, run `git lfs pull`, then right-click the `.uproject` and
  choose *Generate Visual Studio project files*, then build.

## Setting up a new machine

Run these **before** cloning, in this order:

```powershell
winget install --id Git.Git -e          # bundles git-lfs
winget install --id GitHub.cli -e
git lfs install
git config --global core.longpaths true # REQUIRED - see below
gh auth login                           # interactive, browser
git clone https://github.com/groatse/DISMultiplayerTank.git
```

`core.longpaths` is not optional. Some content paths (e.g.
`Content/UI/EasyOptionsMenu/EasyInputPrompts/GraphicContent/MouseAndKeyboard/...`) exceed
Windows' 260-character `MAX_PATH`. Without it, `git clone` fails checkout on ~63 assets with
`Filename too long` and leaves a silently incomplete working tree — the clone *appears* to
succeed. Windows' own `LongPathsEnabled` registry flag is separate and does not cover git.
