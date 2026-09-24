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

---

## Entry 8 — 2026-09-24

### Prompt

> a thing on the score sync: now that the choice has been made with the shells, the 10 kills are actually confirmed from both players, there should be no disagreement, correct?

### Claude's Answer

Confirmed: with victim-adjudicated kills, every death has exactly one authority (the tank that died declares it), and both instances derive both scores from those declarations — so the machines cannot decide differently; at worst one learns the outcome one latency later.

Added one refinement to make it drop-proof: transmit score as state, not events. A death signaled only by a transient "destroyed" appearance could be missed entirely if all packets during the freeze window drop and the tank respawns before a surviving heartbeat. Instead, each tank carries a monotonic cumulative death counter (plus a round number) in its published state; score = opponent's death counter, receivers take the max ever seen. This is idempotent and immune to drops, duplicates, and reordering — a lost packet delays a score tick, never loses it. It also makes the 10-kill map reset deterministic with no extra reset broadcast: both machines watch the same counters and conclude "round over" from identical data, and the round number keeps stale packets from the previous round from polluting the new one.

### User Choices

- Confirmed the score model: kills are single-authority (victim-declared), so no score disagreement is possible — accepted as the basis for the 10-kill reset with the state-based (monotonic death counter + round number) refinement.

---

## Entry 9 — 2026-09-24

### Prompt

> before we go into that, any changes / additions needed for the plan to support more players than 2? that is a possible scope creep I want to keep in mind

### Claude's Answer

Assessed N-player readiness: the design is mostly N-ready by construction, because DIS is a peer-broadcast protocol with no protocol-level "other player" concept, and the authority model is per-entity — shooter-vs-DR'd-tank collision, victim-adjudicated deaths, shell-as-entity, and one-shell-per-tank all scale unchanged.

Two places are secretly 2-player-shaped, both cheap to fix now:

1. **Kill attribution:** a single death counter identifies the death but not who scored. Generalize the published score state to per-killer counters ("entity X has killed me N times" — a small monotonic take-max map); my score = sum of everyone else's counters for my ID. The victim stays the single authority for attribution, consistent with it validating the killing detonation. First-to-10 and round reset still derive deterministically.
2. **Identity:** the pairwise token tiebreak generalizes to a peer registry — everyone broadcasts a session token, slot = your token's rank in the sorted set of known tokens; slot derives spawn point and color. Structurally: keep a map of entity ID to peer state instead of a single "remote tank" pointer; game start is a "≥2 peers present" threshold.

Knock-ons: score UI as a dynamic list; spawn points as a slot-indexed array.

Recommendation: build the data structures N-shaped now (peer registry, per-killer counters, slot-derived spawn/color, list-based score UI) while keeping 2 players the only tested/shipped configuration — later expansion becomes config plus testing, not a refactor. Explicitly out of scope now: late-join-mid-round semantics, N-player testing, lobby flow.

### User Choices

- Flagged >2 players as possible future scope creep to keep in mind; the plan is to be structured so that supporting more players later requires no redesign (pending confirmation of the N-shaped data structure approach).

---

## Entry 10 — 2026-09-24

### Prompt

> alright, let's do an architecture plan with class breakdown first. I want it cleanly separated to folders (with PascalCaseNaming) and Source root only contains what's in there now. The naming of classes should always follow UE standards, and should of the structure "ParentClass/OtherType" + "DISTanks", example "GameModeDISTanks".

### Claude's Answer

Wrote the full architecture plan to `Docs/ArchitecturePlan.MD`. Summary:

- **Layout:** single runtime module; all new code in PascalCase subfolders of `Source/DISMultiplayerTank/`: `Core/`, `Tanks/`, `Shells/`, `Networking/`, `UI/`. Source root unchanged.
- **Classes (all `<Type>DISTanks`, UE prefixes on the C++ class):** `AGameModeDISTanks` (per-instance match state machine, auto-start at ≥2 peers, camera + spawn, 10-kill reset), `APlayerControllerDISTanks` (Enhanced Input, blocked during death freeze), `ATankDISTanks` (one pawn class with owned mode — fixed-timestep Combat movement, DISSendComponent, one-shell limit, victim death adjudication — and ghost mode driven by DISReceiveComponent dead reckoning), `AShellDISTanks` (owned: kinematic steerable flight + impact detection + Detonation; ghost: dead-reckoned visual), `UPeerRegistryDISTanks` (GameInstance subsystem: peer map, token-rank auto-negotiation for slots, per-killer monotonic death counters + round number with take-max merge), `UPDURouterDISTanks` (GameInstance subsystem: sole GRILL DIS touchpoint — UDP bootstrap, PDU routing, ghost spawning, custom state codec), `PDUTypesDISTanks.h` (shared types/constants), `AHUDDISTanks` (Canvas score list, match state text).
- **Rules:** no Unreal replication; single authority per entity; victim-declared kills; state-over-events for scores/identity; N-shaped data with 2-player shipped scope.
- **Risks flagged:** custom-state transport in ESPDU fields vs. fallback Data/Comment PDUs; localhost twin-instance UDP port sharing; plugin dead-reckoning fidelity for fast shells (fallback: manual shell PDU routing).
- **Build order:** 1) local tank + arena, 2) shell + death/respawn local, 3) DIS tank sync, 4) DIS shell + kill flow, 5) identity/auto-start, 6) score + round flow, 7) packaging + docs + video. Each step ends runnable.

### User Choices

- Folder-per-domain source organization with PascalCase folder names; `Source/` root frozen to current contents.
- Class naming convention fixed as "ParentClass/OtherType" + "DISTanks" (e.g. `GameModeDISTanks`) following UE standards.

---

## Entry 11 — 2026-09-24

### Prompt

> why subsystems and not a gameinstance that handles replications/DIS?

### Claude's Answer

Defended the subsystem choice: `UGameInstanceSubsystem`s have the exact same lifetime as a custom GameInstance (created at instance init, survive map loads — which matters for peer identity and DIS sockets across a level-reload round reset), so nothing is lost. What's gained: (1) separation — a DIS-handling GameInstance becomes a god object since there is only one place to put everything, whereas subsystems give the wire (`UPDURouterDISTanks`) and identity/score (`UPeerRegistryDISTanks`) their own single-purpose units with an explicit init dependency via `Collection.InitializeDependency`; (2) typed access anywhere via `GetSubsystem<T>()` instead of a `Cast<UGameInstanceDISTanks>` at every call site; (3) symmetric composition with GRILL DIS, which itself ships as subsystems — our layer sits beside the plugin's, and a pivot on any flagged risk has a one-subsystem blast radius; (4) no door closed — a custom GameInstance can still be added later without touching the subsystems, while the reverse migration (unpicking a fat GameInstance) is the painful direction. Acknowledged the counterpoint that a custom GameInstance would work for a prototype this small, but subsystems cost the same and demonstrate the modern UE idiom, which is part of what is being evaluated.

### User Choices

- Questioned the subsystem-vs-custom-GameInstance choice; subsystem approach stands per the rationale above.

---

## Entry 12 — 2026-09-24

### Prompt

> edit the architecture implementation plan: don't do the ship part, I'll do that apart from the README and technical notes. then for integration tests of the network code I want to have it atleast semi-automated for different lag conditions (high packet loss - high latency). give suggestions.

### Claude's Answer

Updated `Docs/ArchitecturePlan.MD`: packaging and gameplay video/screenshots are now explicitly the user's tasks; Claude still delivers README and technical notes (final build step 8). Added a network test harness section and a new build step 7 for it. Suggested (and planned) approach:

1. **Application-layer network conditioner (`FNetConditionerDISTanks`)** — because all PDUs flow through `UPDURouterDISTanks`, a conditioning stage on the inbound path can apply configurable latency, jitter, drop, duplication, and reordering with a seeded RNG, driven by command line (`-NetSimLatencyMs`, `-NetSimLossPct`, `-NetSimSeed`, ...). Deterministic and reproducible, no OS tooling or admin rights, inert when unconfigured. (UE's built-in packet emulation was ruled out — it only affects the Unreal NetDriver, not GRILL DIS's raw sockets.)
2. **`UAutoPilotDISTanks` (Testing/)** — seeded scripted driver feeding deterministic movement/fire input (including steer-after-fire) so two instances fight unattended via `-AutoPilot`.
3. **Structured log markers + `Tools/RunNetTest.ps1`** — machine-parseable `LogDISTanks` markers (kills with attribution, counter merges, round resets, periodic DR error samples); the script launches two instances with a named lag profile, waits, parses both logs, and asserts cross-instance agreement (identical final death counters, bounded shell lifetime, matching round numbers, DR error percentiles in tolerance). Exit code per profile = semi-automated pass/fail.
4. **Optional one-off cross-check with clumsy (WinDivert)** to validate the app-layer sim against real OS-level packet mangling; manual, not in the automated loop.

Standard profiles defined: Baseline, Latency (150 ms ± 50), Loss (20 %), Nasty (250 ms ± 100 + 30 % loss + duplicates + reorder). Technical notes will include test results per profile.

### User Choices

- Packaging the executable build and producing the gameplay video/screenshots are the user's own tasks; Claude delivers README + technical notes.
- Network integration testing must be at least semi-automated across lag conditions (high packet loss, high latency); accepted suggestions pending review.

---

## Entry 13 — 2026-09-24

### Prompt

> alright, looks good. Now for the in-code documentation, I want .h to have a single sentence explanation for each function. All variables (even local) to be kept descriptive and in PascalCase and UE5 standard. Documentation in .cpp also kept to one sentence length (no 5 row Claude comments). Start phase 1.

(Mid-phase correction: engine is at `D:\UE4\UE_5.7`, not the default Epic Games folder.)

### Claude's Answer

Implemented phase 1 (local gameplay core) under the stated documentation standards. New classes:

- `ATankDISTanks` (Tanks/) — Combat-style pawn: box collision root + placeholder cube hull and hull-fixed barrel; fixed-timestep (60 Hz substep, hitch-capped accumulator) movement with rotate-in-place + forward/back thrust; swept moves so walls block; input setters clamped to [-1,1]; movement-freeze flag and fire stub ready for phase 2.
- `AArenaDISTanks` (Core/) — procedural arena, no map asset needed: 1500x2000 floor with top at Z=0, four boundary walls, two symmetric center obstacles, movable directional light, fixed top-down camera (FOV 50 at 3200 cm for a pseudo-orthographic look), and a slot-indexed spawn transform array.
- `APlayerControllerDISTanks` (Core/) — Enhanced Input built entirely in code (runtime-created mapping context/actions, no content assets): W/S + Up/Down thrust, A/D + Left/Right turn (negate modifiers), Space fire; Completed bindings zero the axes on release.
- `AGameModeDISTanks` (Core/) — sets pawn/controller classes, lazily spawns the arena, spawns the pawn at slot 0 (peer-registry slots come in phase 5), and sets the arena camera as view target.

Supporting changes: module root added to include paths in Build.cs; `DefaultEngine.ini` boots the engine's Entry map with `GlobalDefaultGameMode` pointing at our GameMode (avoids authoring a binary .umap; a real map can replace this later) and disables auto-exposure. Verified: editor target compiles; standalone launch confirmed `Game class is 'GameModeDISTanks'`, and a screenshot showed the lit arena with walls, obstacles, and the tank at its spawn slot. VS project files regenerated.

### User Choices

- In-code documentation standards: one-sentence function docs in headers, PascalCase descriptive variables everywhere (locals included), one-sentence max comments in .cpp files.
- Corrected engine location to `D:\UE4\UE_5.7` (saved to Claude's memory).

---

## Entry 14 — 2026-09-24

### Prompt

> start phase 2

(Mid-phase correction: "clearly your input method is not functioning, I can move the tank and shoot and hits register" — after Claude spent several cycles trying to verify firing by injecting synthesized keystrokes into the game window.)

### Claude's Answer

Implemented phase 2 (shell & death, all local):

- `AShellDISTanks` (Shells/) — kinematic shell on the same fixed-timestep pattern as the tank: sphere collision + placeholder sphere mesh; flies at 900 cm/s and steers with the owner tank's live turn input (the steerable-shell mechanic, 90 deg/s); swept movement detonates on any blocking hit — walls kill the shell, an enemy tank takes the hit; shells ignore their shooter and pass through other shells like the original; 5 s failsafe lifetime.
- `ATankDISTanks` — `RequestFire` spawns the shell at the muzzle with one-shell-at-a-time enforcement (weak pointer to the active shell) and blocks firing while dying; `HandleShellHit` starts the classic death sequence: player control frozen, forced slide in a random direction at 300 cm/s for 1.5 s (swept, so walls stop the slide), then control returns; invulnerable while the sequence runs.
- `AGameModeDISTanks` — temporarily spawns an unpossessed practice-target tank at slot 1 (config flag, removed when ghost tanks arrive in phase 3).
- New `LogDISTanks` log category (module header) with structured markers — `Fire`, `ShellImpact`, `TankDeath`, `TankRespawned` — which the phase-7 network test harness will parse.

Verification: compiles clean; visual launch showed both tanks in the arena; Claude's attempts to fire via synthesized keystrokes (SendKeys with confirmed foreground focus and viewport click) never reached Enhanced Input and were abandoned — the user playtested manually and confirmed movement, shooting, and hit registration all work. Temporary input diagnostics were removed after the finding; the lesson (never verify UE gameplay via injected keystrokes; use user playtests or the future autopilot) was saved to Claude's memory.

### User Choices

- Confirmed phase 2 works by playtesting (movement, firing, hits registering).
- Established that gameplay verification is done by the user's manual testing or the planned autopilot harness, not synthesized input.

---

## Entry 15 — 2026-09-24

### Prompt

> start phase 3

### Claude's Answer

Implemented phase 3 (DIS tank sync). First mapped the GRILL DIS plugin API with an exploration agent, which confirmed the plan's preferred integration: bypass `ADISGameManager`/send/receive components entirely (their receive routing requires a content asset) and use only the plugin's two GameInstance subsystems — `UUDPSubsystem` (sockets are SO_REUSEADDR-reusable; receive must set `bAllowLoopback=true` for two instances on one machine) and `UPDUProcessor` (auto-parses received bytes into typed PDU delegates). GeoReferencing avoided: both peers ride arena-local meters in the ECEF fields, a documented prototype simplification.

New code:
- `PDUTypesDISTanks.h` (Networking/) — protocol conventions: exercise/site IDs, cm↔m conversion, DIS relative-timestamp flattening with wrap-aware newer-than comparison, entity ID formatting.
- `UPDURouterDISTanks` (Networking/) — the sole GRILL DIS touchpoint. Opens a broadcast send socket and a shared-port loopback-enabled receive socket (port 3000, `-DISPort=`/`-DISBroadcastAddress=` overrides); publishes the local tank per the Entry-7 design: mirrors the receivers' constant-velocity prediction locally and sends an ESPDU only when reality drifts >0.1 m or >2° from it, or on a 1 s heartbeat; routes received ESPDUs (filtered by exercise ID and own-entity echo) to ghost tanks, spawning them for unknown entity IDs; drops stale/out-of-order PDUs by timestamp; times out and destroys ghosts unheard for 10 s. Random DIS application ID as interim identity; `-PlayerSlot=` / app-parity as interim spawn slot (both replaced by phase-5 negotiation).
- `ATankDISTanks` — ghost mode: `InitAsGhost`/`ApplyRemoteTankState`, per-frame constant-velocity dead reckoning with capped extrapolation (1 s) plus exponential smoothing toward the prediction; owned mode now tracks post-sweep step velocity that feeds the published DR state.
- GameMode registers the local tank with the router and spawns at the router's interim slot; practice target now off by default. Build.cs gained `DISRuntime`, `Sockets`, `Networking`. Added `Tools/LaunchTwoInstances.ps1`.

Verified end-to-end on localhost: two instances each opened sockets, published, and spawned a ghost for the other (log markers `RouterInitialized`/`SocketsOpened`/`GhostTankSpawned` on both sides with correct mirrored positions); screenshot showed both tanks rendering in both windows.

### User Choices

- (None this exchange — phase executed per the standing plan.)

---

## Entry 16 — 2026-09-24

### Prompt

> reorganize files into two root folders private (for .cpp) and public (for .h) with the same subfolder structure

### Claude's Answer

Reorganized the module into the standard UE `Public/`/`Private/` layout: all headers under `Public/{Core,Tanks,Shells,Networking}/` plus `Public/DISMultiplayerTank.h`, all .cpp files under the mirrored `Private/` tree, `Build.cs` at the module root. Removed the now-obsolete module-root include-path workaround from `Build.cs` — UBT auto-registers Public/Private, so all existing `#include "Folder/File.h"` paths kept working unchanged. Rebuilt successfully and regenerated IDE project files. `Docs/ArchitecturePlan.MD` folder diagram updated to match.

### User Choices

- Module source layout: standard UE Public (.h) / Private (.cpp) root folders with mirrored subfolder structure.

---

## Entry 17 — 2026-09-24

### Prompt

> before we go to phase 4, isn't that public private thing kinda obsolete ? why does UE/Epic insist upon it?

### Claude's Answer

Conceded that for a single-module game the split is mostly costume — no other module consumes our Public headers, so the compiler treats both folders identically. Explained why it is not obsolete for Epic: in UE the split is a build-system-enforced module boundary, not OOP visibility. UnrealBuildTool adds only a dependency's `Public/` folder to the dependent's include paths, so `Private/` headers cannot be included across modules at all. At engine scale (~1,000 modules) that provides: (1) a real API surface paired with `MODULENAME_API` DLL-export macros; (2) build-graph hygiene — private edits never recompile dependent modules, keeping iteration fast; (3) distribution without source — installed engines and binary marketplace plugins ship Public headers plus compiled libs while Private doesn't exist on disk (GRILL DIS's prebuilt OpenDIS core being a nearby example). Kept it for this project because the cost is zero post-migration, it removed the Build.cs include workaround, it pre-draws the boundary if Networking ever becomes its own module, and reviewers read it as idiomatic UE.

### User Choices

- Questioned the Public/Private convention; accepted keeping it with the understanding that for this project it is idiom rather than necessity.
