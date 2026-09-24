# Architecture Plan — DISMultiplayerTank

Top-down Atari Combat-style 2-player tank prototype, synchronized over DIS (GRILL DIS plugin, UE 5.7). This document is the class breakdown and build order; design rationale lives in `ClaudeProjectLog.MD` entries 6–9.

## Module & folder layout

Single runtime module (`DISMultiplayerTank`) — `Source/` root keeps exactly its current contents. All new code goes into PascalCase subfolders of the module:

```
Source/DISMultiplayerTank/
├── DISMultiplayerTank.Build.cs        (existing)
├── DISMultiplayerTank.h / .cpp        (existing)
├── Core/          — game framework classes (mode, controller)
├── Tanks/         — tank pawn
├── Shells/        — shell actor
├── Networking/    — DIS integration: peer registry, PDU routing, codecs, shared types
└── UI/            — HUD
```

## Naming convention

`"ParentClass/OtherType" + "DISTanks"` (e.g. `GameModeDISTanks`), with standard UE prefixes (`A`/`U`/`F`) on the C++ class, none on the file name.

## Class breakdown

### Core/

**`AGameModeDISTanks`** (AGameModeBase)
Match orchestration on each instance (there is no server — every instance runs its own GameMode):
- Match state machine: `WaitingForPeers → Playing → RoundResetting`.
- Starts play when the peer registry reports ≥2 peers (auto-start, no menu).
- Spawns the local tank at the slot-derived spawn point; spawns the fixed top-down camera and sets the view target.
- Watches score state; at 10 kills triggers deterministic round reset (repositions local tank, bumps round number via registry).

**`APlayerControllerDISTanks`** (APlayerController)
- Enhanced Input mapping: turn left/right, forward/back thrust, fire.
- Forwards input to the tank pawn; input is ignored while the tank is in its death-freeze state.

### Tanks/

**`ATankDISTanks`** (APawn)
One class for both owned and ghost tanks, differentiated by ownership:
- **Owned mode:** classic Combat movement (rotate in place + forward/back along facing) on a fixed timestep; wall collision blocks movement; carries the GRILL DIS **DISSendComponent** (threshold-triggered ESPDUs + heartbeat + dead-reckoning parameters per log Entry 7). Enforces one-shell-at-a-time. Adjudicates its own death: validates incoming Detonation PDUs against its true position with tolerance, then publishes death (attributed to the shooter) via the peer registry state.
- **Ghost mode:** no input/physics; position driven by GRILL DIS **DISReceiveComponent** dead reckoning with smoothing.
- Death sequence (both modes render it; only the owner decides it): movement freeze + forced slide in a random direction, brief post-respawn grace, then control returns.

### Shells/

**`AShellDISTanks`** (AActor)
- **Owned mode (shooter's instance):** kinematic fixed-timestep flight; steering — turning the owner tank steers the shell in flight (first-iteration feature); carries DISSendComponent (fast heartbeat ~10–15 Hz, threshold updates fire immediately on steer). Detects impact: walls → Detonation (no ricochet); exact-shell vs dead-reckoned enemy tank → Detonation PDU targeting that entity ID.
- **Ghost mode:** dead-reckoned via DISReceiveComponent; purely visual, no collision authority.

### Networking/

**`UPeerRegistryDISTanks`** (UGameInstanceSubsystem)
The N-player-shaped heart of identity and score (log Entry 9):
- Maintains a map: DIS entity ID → peer state (`FPeerDISTanks`).
- **Auto-negotiation:** each instance generates a random session token at launch and publishes it in its state; slot = rank of own token among all known tokens (deterministic, no handshake, race-free); slot derives spawn point and tank color. Token ties re-roll.
- **Score state:** per-killer monotonic death counters ("entity X has killed me N times") + round number, published as state and merged take-max on receive — idempotent under drops, duplicates, reordering. Score = sum of everyone else's counters for my entity ID.
- Emits events: peer joined/lost (timeout on last-heard), scores changed, round-reset threshold reached.

**`UPDURouterDISTanks`** (UGameInstanceSubsystem)
The single place that touches GRILL DIS plumbing:
- Bootstraps UDP settings (broadcast address, port, localhost multi-instance support) and the plugin's PDU processor / DIS game manager.
- Routes received PDUs: ESPDUs for unknown entity IDs → spawn ghost tank/shell; Fire/Detonation PDUs → dispatch to the relevant actors; custom state (session token, death counters, round number) → peer registry.
- Owns the encode/decode of custom state onto the wire (preferred: standard ESPDU fields; fallback: Data/Comment PDUs — see Risks).

**`PDUTypesDISTanks.h`** — shared plain types: `FPeerDISTanks` (token, entity ID, slot, last-heard, death counters, round), entity-type constants, tuning constants (heartbeats, thresholds, tolerances).

### UI/

**`AHUDDISTanks`** (AHUD)
- Canvas-drawn score list (dynamic list, not two fixed fields — N-shaped) and match state text ("Waiting for opponent…", round reset banner). Zero content-asset dependencies, pure C++, reviewable.

## Content (no classes)

- One arena map: floor, four boundary walls + a few inner walls (placeholder box meshes), spawn point array indexed by slot, exponential height fog not needed — flat unlit look is fine.
- Tank/shell visuals: placeholder cube/box components colored by slot.

## Key architectural rules

1. **No Unreal replication anywhere** — all cross-instance state flows as DIS PDUs.
2. **Single authority per entity:** each instance simulates only what it owns; everything remote is dead-reckoned, never corrected locally.
3. **Kills are victim-declared** (standard DIS split): shooter detonates, victim validates and publishes its own death + attribution.
4. **State over events** for anything that must not be lost (scores, round number, identity).
5. **N-shaped data, 2-player shipped:** peer maps, per-killer counters, slot arrays, list UI — but only 2 players tested/supported.

## Risks / open implementation questions

- **Custom state transport:** whether GRILL DIS's send component allows stuffing token/counters into ESPDU appearance/capabilities fields, or whether we send a parallel low-rate Data/Comment PDU from `UPDURouterDISTanks`. Resolved during step 3 below; both paths keep the same registry API.
- **Localhost twin instances:** two processes sharing a UDP broadcast port need socket reuse; verify GRILL DIS supports it, else document distinct-port config for local testing.
- **Plugin DR fidelity for fast shells:** if plugin dead reckoning proves too coarse for shell speeds, fall back to routing shell ESPDUs manually through `UPDURouterDISTanks`.

## Build order

1. **Local gameplay core** — folders, `ATankDISTanks` movement + camera + arena map with walls; playable single instance.
2. **Shell & death** — `AShellDISTanks` flight/steering, wall impact, hit → freeze + random-slide respawn, one-shell limit; all local.
3. **DIS tank sync** — `UPDURouterDISTanks` bootstrap, tank ESPDUs out, ghost tank spawn + dead reckoning in; two instances see each other move.
4. **DIS shell & kill flow** — shell as DIS entity, Fire/Detonation PDUs, victim adjudication + published death.
5. **Identity & auto-start** — `UPeerRegistryDISTanks` token negotiation, slots, spawn/color assignment, auto-start at ≥2 peers.
6. **Score & round flow** — per-killer counters, round number, HUD, 10-kill reset.
7. **Ship** — packaged build, README, technical notes, gameplay/sync video.

Each step ends in a runnable, committable state.
