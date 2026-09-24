# Claude Project Log

Running log of the collaboration between Touko and Claude Code on the DISMultiplayerTank project (lead engineer job interview project). Each entry records the user's prompt, Claude's answer (code excluded), and any decisions made by the user.

---

## Entry 1 — 2026-09-24

### Prompt

> this is a project that I'm doing as part of a job interview for a lead engineer. start ClaudeProjectLog.MD in the root where each input prompt to you is recorded, as well as all answers you give (excluding code) and all choices I make. Include this prompt in it as well.

### Claude's Answer

Created `ClaudeProjectLog.MD` in the project root. Going forward, every prompt, every non-code answer, and every user decision will be appended to this file as a new entry.

### User Choices

- Decided to maintain a project log (`ClaudeProjectLog.MD`) documenting the full collaboration for the interview project.

---

## Entry 2 — 2026-09-24

### Prompt

> yes, set up git with the UE5 gitignore - include the log in the repo

### Claude's Answer

Initialized a git repository in the project root and created a UE5-appropriate `.gitignore` excluding generated/cache directories (`Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, `Build/`), IDE folders (`.vs/`, `.idea/`, `.vscode/`), the auto-generated `.sln`, and OS cruft. Made the initial commit (`99eb760`) containing 12 files: the C++ source skeleton, config files, the `.uproject`, the `.gitignore`, and `ClaudeProjectLog.MD`. Noted that `Content/` is currently empty, so nothing from it was staged — expected for a fresh C++ project.

### User Choices

- Accepted Claude's suggestion to set up version control with a UE5 `.gitignore`.
- Chose to track `ClaudeProjectLog.MD` in the repository.

---

## Entry 3 — 2026-09-24

### Prompt

> setup lfs. also, only run sync on my command (copy skill /sync from D:\UE5\LoadedDiceFaceOff\.claude\skills\sync)

### Claude's Answer

Ran `git lfs install` and added a `.gitattributes` tracking UE binary assets (`.uasset`, `.umap`) and common imported media (textures, audio, FBX, fonts, DLLs) through LFS. Copied the `/sync` skill from the LoadedDiceFaceOff project into `.claude/skills/sync/`, with two adaptations: the skill description now states it must only be invoked on explicit user command (never proactively), and the old project's clone URL was replaced with a placeholder until this repo has a remote. The skill pulls before pushing and refuses to auto-resolve diverged histories, since binary UE assets cannot be merged.

### User Choices

- Adopted Git LFS for binary assets.
- Reused the `/sync` workflow from the LoadedDiceFaceOff project, with the rule that sync runs only on explicit command.

---

## Entry 4 — 2026-09-24

### Prompt

> create the remote, private

### Claude's Answer

Created the private GitHub repository `groatse/DISMultiplayerTank` with `gh repo create`, added it as `origin`, and pushed `main` with tracking set up. Replaced the placeholder clone URL in the `/sync` skill with the real one and pushed that follow-up commit as part of completing the setup.

### User Choices

- Chose a private repository for the interview project.
