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

---

## Entry 5 — 2026-09-24

### Prompt

> create another log - ProjectMainLog.MD - which has main tasks completed (like from above it would include two lines: 1) ClaudeProjectLog setup - contains communication and choices with Claude. 2) Git repo setup - for version control and deliverables. In the future it will have all new classes created and their main functionality along with design / architectural choices made.

### Claude's Answer

Created `ProjectMainLog.MD` in the repo root with the two requested entries (ClaudeProjectLog setup, Git repo setup) and a header describing its purpose. Going forward, each completed main task — new classes with their main functionality, and design/architectural choices — will be appended there, keeping it a concise deliverable-level summary separate from this verbose conversation log.

### User Choices

- Established a two-log structure: `ClaudeProjectLog.MD` for the full conversation record, `ProjectMainLog.MD` for high-level completed tasks and architectural decisions.

---

## Entry 6 — 2026-09-24

### Prompt

> now read this and get acquinted with the project. ask me for clarifications or design choices. file:///C:/Users/groat/Downloads/Unreal%20Engine%20Take-Home%20Assignment_%20Atari%202600%20Combat-Style%20Multiplayer%20Prototype-230926.pdf also be aware, that the game is top-down, we will be using the GrillDIS plugin, we will have walls and respawn is like the old game (movement freeze, and a forced move to a random direction)

### Claude's Answer

Read the assignment PDF: a 2-player Atari Combat-style tank prototype in UE 5.7/5.8, synchronized over the DIS protocol (GRILL DIS plugin) instead of Unreal replication — tank identity/position/orientation/velocity, fire events, and hit/destruction events must flow as DIS PDUs, with reasonable handling of delayed/dropped/out-of-order updates. Deliverables: source, executable build, README, technical notes (authority model, sync approach, assumptions, limitations, AI-tool usage), and video/screenshots. Evaluation favors a small reliable prototype with readable code and clear tradeoffs.

Verified the project is already prepared: UE 5.7 with `GRILLDISForUnreal` and `GeoReferencing` plugins enabled.

Stated working assumptions: classic Combat movement (rotate in place + forward/back, gun fixed to hull, no independent turret), fixed top-down camera showing the whole arena, dead reckoning with smoothing for remote entities.

Asked four design questions: hit/kill authority model, shell synchronization method, instance identity assignment, and classic gameplay feature scope.

### User Choices

- **Setting (from prompt):** top-down game, walls included, GrillDIS plugin, classic Combat respawn (movement freeze + forced move in a random direction).
- **Hit authority — standard DIS split:** shooter simulates and publishes its shells (Fire + Detonation PDUs); the victim adjudicates damage to its own tank from incoming Detonations and publishes its own destroyed state. Each machine stays fully authoritative over its own entities.
- **Shell sync — full DIS entity:** in the original game the shell is steerable (turning the tank steers the shell in flight), which requires the shell to be a live entity publishing Entity State PDUs, not a fire-and-forget event. User acknowledged this is prone to errors under lag/dropped frames affecting hit authority, but wants the steerable-shell ability in the first iteration.
- **Instance identity — auto-negotiate with a failsafe against race conditions:** the game must run with minimal clicking — launch two instances on the network and they automatically start a game, no menu.
- **Gameplay features:** one shell at a time per tank; score display; keep score; map reset at 10 kills. (No shell ricochet off walls, no round timer.)

---

## Entry 7 — 2026-09-24

### Prompt

> a thing on your two things number 1. what is your imagined DIS PDU flow for the steerable shells? Is it Shooter entity state update PDU for turning -> shell receives and updates its direction to the same direction -> Victim receives shooter PDU on turning and calculates shell location or victim receives shell PDU on location each frame and calculates for collision? what is the maximum deterministical and least lag prone setup that results in the least variation of visual events between players?

### Claude's Answer

Compared three shell-sync flows:

1. **Deriving the shell from the shooter's tank ESPDUs** (victim integrates the shooter's orientation stream to reproduce the shell path) — rejected: it is input-streaming lockstep over an unreliable transport; every delayed/dropped/reordered orientation update permanently bends the victim's copy of the trajectory and the error compounds over flight time.
2. **Shell ESPDU every frame with raw-position collision** — rejected as brute force: dropped packets still leave gaps needing extrapolation, raw positions stair-step under jitter, and frame rate is not a shared clock between machines.
3. **Recommended (DIS-native): shell as its own entity with dead reckoning + threshold-triggered ESPDUs.** ESPDUs carry position *and velocity*, so the receiver self-predicts: Fire PDU + initial ESPDU on launch; the shooter mirrors the receiver's dead-reckoning model locally and emits a fresh ESPDU the instant the true shell deviates past a threshold (i.e., immediately on steering) — so remote error is bounded by one network latency, never accumulated. A fast shell heartbeat (~10–15 Hz vs. the DIS default 5 s) repairs dropped corrections within ~100 ms; out-of-order ESPDUs are discarded by timestamp. Shell physics runs on a fixed timestep on the owning machine so its path is frame-rate independent.

Collision: any shell-vs-tank test pairs one owned and one dead-reckoned entity, so pick the smaller DR error — the shooter tests its exact shell against the DR'd victim tank (tanks are slow, so that error is tank speed x latency, i.e., tiny), sends a Detonation PDU targeting the victim, and the victim validates against its true position with tolerance before publishing its destruction. Matches the standard DIS authority split already chosen; kills are single-authority so screens cannot disagree on outcomes. Net visual variation: straight shells are pixel-identical (constant-velocity DR is exact); steered shells differ only for ~1 RTT after each turn input.

### User Choices

- Probed the steerable-shell PDU flow for the most deterministic, least lag-prone design before implementation (decision on the recommended flow pending user confirmation).
