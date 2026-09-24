# Technical Notes — DISMultiplayerTank

How this prototype synchronizes an Atari *Combat*-style tank duel over the DIS protocol: the message flow, the authority model, what happens under packet loss, what was deliberately simplified, and what would come next. Written for reviewers; the full decision history including rejected alternatives lives in `ClaudeProjectLog.MD`.

## 1. Architecture at a glance

Single C++ module, no content assets (arena, input, HUD are all code-constructed), no Unreal replication anywhere. Eight gameplay/infra classes:

| Class | Role |
|---|---|
| `UPDURouterDISTanks` | **The only code touching GRILL DIS.** Sockets, PDU publish/receive/routing, ghost lifecycle, victim-side kill adjudication, net conditioner host |
| `UPeerRegistryDISTanks` | Identity (session token), peer set, deterministic slots, score counters, round state — deliberately free of DIS types |
| `ATankDISTanks` | One pawn class, two modes: owned (fixed-timestep simulation) or ghost (dead-reckoned mirror) |
| `AShellDISTanks` | Steerable shell, same owned/ghost split |
| `AGameModeDISTanks` | Per-instance match orchestration (every instance runs its own — there is no server) |
| `AArenaDISTanks` | Procedural arena: floor, walls, obstacles, light, top-down camera, N slot-indexed spawns |
| `AHUDDISTanks` | Canvas score row, round number, waiting banner |
| `UAutoPilotDISTanks` / `FNetConditionerDISTanks` | Test harness: unattended driver + seeded inbound loss/latency/jitter/duplication |

## 2. DIS integration choices

**Plugin layers used:** GRILL DIS's two GameInstance subsystems only — `UUDPSubsystem` (sockets) and `UPDUProcessor` (bytes → typed PDU structs → delegates). The plugin's higher-level `ADISGameManager` / send / receive components were **bypassed**: their receive-side routing requires a content-asset entity-type mapping, and this project is deliberately asset-free C++. The cost is re-implementing threshold publishing and dead reckoning (~150 lines); the benefit is a single reviewable file owning the entire wire protocol.

**Transport:** UDP broadcast on port 3000 (configurable). Sockets are opened `SO_REUSEADDR` with loopback delivery enabled, so two instances on one machine share the port and hear each other; own packets echo back and are filtered by application ID. LAN play needs no configuration; directed broadcast is available via `-DISBroadcastAddress`.

**Coordinates — documented simplification:** DIS specifies geocentric ECEF meters. Both peers here write *arena-local* meters (Unreal axes, cm/100) into the ECEF fields and interpret them identically, skipping the GeoReferencing plugin entirely. Wrong for interop with real DIS tools, right for a two-instance prototype; section 7 covers the upgrade path.

**Identity scheme:** Exercise 1, Site 1. Each instance's random session token (1–65535) doubles as its DIS **Application ID**. Entity numbers: 1 = the instance's tank, 100+ = its shells (incrementing per shot). So `1:12735:1` is instance 12735's tank and `1:12735:100` its first shell.

## 3. Message and event flow

Three PDU types cross the wire — Entity State, Fire, Detonation — all sent through one router and received through one conditioned path.

**Movement (Entity State PDU).** The owning instance mirrors, locally, the exact constant-velocity prediction its receivers run, and publishes only when reality drifts from that prediction (>0.1 m position or >2° yaw) or a heartbeat elapses (1 s tank, 100 ms shell). A straight-driving tank therefore costs ~1 packet/s while remaining glass-smooth remotely; the *moment* it turns, a corrective ESPDU goes out, so remote error is **bounded by one network latency, never accumulated**. Receivers dead-reckon (DIS FPW: position + velocity extrapolation, capped at 1 s), smooth toward the prediction exponentially, and snap across corrections larger than 4 m (respawns, round resets). Out-of-order updates are dropped by DIS timestamp (wrap-aware compare); stale-entity ghosts time out (10 s tank, 2 s shell).

**Firing and steerable shells.** Fire press → local shell spawns immediately (no round trip — the shooter owns it) → one **Fire PDU** (shooter, munition ID, position, velocity) → remote side seeds a ghost shell instantly from the Fire PDU, one latency earlier than the first ESPDU could arrive. While flying, the shooter's turn input steers the owned shell; each steer changes velocity, trips the threshold, and publishes within a frame. Ghost shells are **visual-only** (collision disabled) — the non-owner never adjudicates with them.

**Kills — the standard DIS authority split.** The shooter's *exact* shell sweeps against the *dead-reckoned* victim tank (the smallest possible DR error: slow tank × latency, vs. fast shell). On impact the shooter emits a **Detonation PDU** (result: EntityImpact + target ID; walls → EnvironmentObjectImpact; lifetime expiry → Dud) and destroys its shell — but *never decides the kill*. The victim receives the Detonation and validates it against its true position (250 cm tolerance, not already dying). Only then does it run the death sequence (control freeze + bounce away from the killer within a 120° arc + random spin) and publish `Damage=Destroyed` in its appearance, which the shooter's ghost mirrors by darkening. Outcomes are single-authority: screens cannot disagree about a kill, only learn it one latency apart. Rejected detonations are logged with reasons.

**Score — state, not events.** Each tank's ESPDU carries DIS articulation records: record type 0 = current round number, one record per opponent = "that app has killed me N times" (monotonic, per round). The victim owns attribution (consistent with owning the death). Receivers take-max merge, so drops, duplicates, and reordering can delay a score tick but never lose or double it. Any score change forces an immediate publish rather than waiting out the heartbeat. An instance's score for app A = the sum of everyone's counters crediting A.

**Round reset without a reset message.** All instances watch identical counters; whichever first computes a score ≥ 10 increments its round number and clears counters. The higher round number propagating in ordinary published state makes every other instance adopt the reset; counters tagged with an older round are ignored. Each instance repositions only its own tank (aborting any mid-death sequence); ghosts follow via DR + snap.

**Identity & auto-start.** No lobby, no handshake: every instance broadcasts state immediately; hearing a new application ID adds a peer. **Slot = rank of your token in the sorted set of all known tokens** — every machine computes the same ranking independently, so simultaneous launches cannot race. Match starts (tank unfreezes, repositions to its slot, gets its slot color) the moment ≥1 peer is known; peer loss (10 s silence) returns to waiting. Verified with three instances: slots 0/1/2 converge, including mid-negotiation re-slotting.

## 4. Handling delayed, dropped, out-of-order traffic

| Failure | Mechanism |
|---|---|
| Dropped movement update | Receiver keeps extrapolating on last velocity; heartbeat (1 s / 100 ms) repairs within one period; error stays bounded by latency |
| Dropped Fire PDU | Shell ESPDUs at 10 Hz spawn the ghost anyway (100 ms later than ideal) |
| Dropped Detonation | Ghost shell removed by 2 s silence failsafe; if the *victim* missed it, the kill genuinely didn't happen — single authority means no divergence |
| Dropped score update | Counters are cumulative state repeated with every publish; take-max merge recovers on the next packet |
| Duplicated packet | ESPDUs idempotent; score merge take-max; detonation for an already-dying tank rejected (`AlreadyDying`) |
| Reordered packet | DIS timestamp compare drops stale ESPDUs (wrap-aware); score/round state monotonic |
| Peer death/quit | Ghost timeout → peer removed → match returns to waiting state |

### Measured results (semi-automated harness, 90 s autopilot combat per profile, both instances conditioned)

| Profile | Conditions | Result | DR error (worst mean / worst max) |
|---|---|---|---|
| Baseline | clean localhost | PASS | 23 cm / 40 cm |
| Latency | 150 ms ± 50 ms | PASS | 19 cm / 1600 cm¹ |
| Loss | 20 % loss | PASS | 18 cm / 61 cm |
| Nasty | 250 ms ± 100 ms, 30 % loss, 5 % dup | PASS | 20 cm / 257 cm |

Assertions per run: both instances reach `MatchStarted`, no fatal errors, final cross-instance score views converge, DR mean error < 400 cm. The Nasty worst-case correction (257 cm) matches the design prediction: error ≈ latency × speed, then corrected — never accumulating.

¹ Single-sample spike from a kill event: the victim's death bounce is an intentional teleport-scale correction that the sampler counts as error; the ghost's 4 m snap rule absorbs it in one frame. Steady-state means tell the real story.

## 5. Assumptions

- Both instances run this exact build (protocol conventions are compile-time constants: exercise/site IDs, entity numbering, articulation-record meaning).
- LAN-grade transport (broadcast reaches peers; no NAT traversal).
- Instances' world clocks are unrelated — only *relative* DIS timestamps are used, and only for same-entity ordering.
- One shell per tank (the classic rule) keeps munition tracking to a single live entity per instance.

## 6. Limitations

- **Local-frame coordinates in ECEF fields** — interops only with itself, not with real DIS simulators (deliberate; see §2).
- **Token collision:** two instances rolling the same 16-bit token (1 in 65,535) would alias; detected nowhere. Acceptable odds for a prototype.
- **Relative timestamps wrap hourly**; the wrap-aware comparison handles one wrap, not multi-hour stale packets.
- **Steerable shell + latency:** under heavy lag the shooter can see a graze that the victim's validation rejects (logged `OutOfTolerance`) — the honest cost of victim authority; tolerance is 250 cm.
- **Net conditioner is inbound-only** — run both instances with the same profile for symmetric conditions.
- **2 players tested and supported**; data structures, negotiation, spawns, and colors are N-shaped (verified once with 3 instances) but N>2 is not a supported configuration: no late-join-mid-round semantics, no N-player soak testing.
- Score HUD/UI is placeholder-grade by design (assignment prioritizes sync correctness).

## 7. With more time

1. **True geodesy:** place a `GeoReferencingSystem`, convert through `UDIS_BPFL`, and the prototype becomes interoperable with standard DIS tooling (the router is the only file that changes).
2. **Richer dead reckoning:** RVW/RPW algorithms with angular velocity + acceleration for curved-path extrapolation (plugin's `UDeadReckoning_BPFL` already implements them), and an interpolation-delay buffer for jitter-proof smoothness instead of pure exponential smoothing.
3. **Acknowledged event channel** for Fire/Detonation (small resend window) rather than relying on failsafes.
4. **N-player hardening:** late-join round state catch-up, per-peer round votes, soak tests with 4+ instances.
5. **Round timer + match flow polish** (the original's 2:16 clock), audio/FX, packaged-build CI running the net-test profiles per commit.
6. **OS-level validation** of the conditioner against clumsy/WinDivert once, to certify the app-layer simulation.

## 8. Third-party code and assets

- **GRILL DIS for Unreal** (GRILL, engine plugin; bundles the OpenDIS6 C++ library) — UDP sockets and PDU marshalling. Used strictly through its two subsystems from `UPDURouterDISTanks`.
- **Unreal Engine built-ins:** BasicShapes meshes/material (placeholder visuals), Enhanced Input, engine fonts. No other third-party code or assets.

## 9. AI/LLM tool usage

This project was built with **Claude Code (Anthropic, Claude Fable 5 model)** as the implementing engineer under the project owner's direction — an explicit subject of the assignment's evaluation:

- **All C++ source, config, PowerShell tooling, and documentation** (including this file) were written by Claude Code.
- **Design decisions were made by the project owner**, with Claude presenting analyzed options: hit authority model, shell-as-DIS-entity for steerability, auto-negotiated identity, score/reset semantics, death-bounce behavior, N-player scope. The complete prompt-by-prompt record — every question, answer, alternative, and decision — is in `ClaudeProjectLog.MD` (23 entries); the task-level summary is `ProjectMainLog.MD`.
- **Verification was layered:** Claude ran compile/launch/log-marker/pixel-sampling checks and the automated net-test harness; the project owner playtested every gameplay-feel milestone (movement, firing, kills, lag behavior) — including catching a session-lag issue that log analysis then attributed to renderer overload rather than the network path.
- Packaging and the gameplay video were produced by the project owner.
