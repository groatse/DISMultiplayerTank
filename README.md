# DISMultiplayerTank

A 2-player Atari 2600 *Combat*-style tank battle prototype for Unreal Engine 5.7, synchronized over the **DIS protocol** (IEEE 1278, via the GRILL DIS for Unreal plugin) instead of Unreal's built-in replication. Two game instances discover each other on the network, negotiate player slots automatically, and fight with steerable shells, classic freeze-and-bounce deaths, and a 10-kill round reset — all state flowing as DIS PDUs over UDP broadcast.

Technical design notes: [`Docs/TechnicalNotes.MD`](Docs/TechnicalNotes.MD). Development history: [`ProjectMainLog.MD`](ProjectMainLog.MD) (task level) and [`ClaudeProjectLog.MD`](ClaudeProjectLog.MD) (full prompt/decision log).

## Requirements

- **Windows 10/11**, Visual Studio 2022 with C++ game development workload
- **Unreal Engine 5.7** (launcher build)
- **GRILL DIS for Unreal** plugin installed **into the engine** (free on Fab; the project references it as `GRILLDISForUnreal`)
- Git with **Git LFS** (`git lfs install` before cloning)

## Build

```powershell
git config --global core.longpaths true   # once per machine
git clone <this repo>
cd DISMultiplayerTank
# generate project files: right-click DISMultiplayerTank.uproject -> "Generate Visual Studio project files"
# or from a shell (adjust engine path):
& "<UE_5.7>\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -project="<full path>\DISMultiplayerTank.uproject" -game -rocket
# build (or open the .sln and build DISMultiplayerTankEditor):
& "<UE_5.7>\Engine\Build\BatchFiles\Build.bat" DISMultiplayerTankEditor Win64 Development -project="<full path>\DISMultiplayerTank.uproject"
```

There are no content assets to cook beyond engine primitives — the arena, input, and HUD are all constructed in C++.

## Run

**Two instances on one machine** (the quickest demo):

```powershell
.\Tools\LaunchTwoInstances.ps1
```

Both windows go from "WAITING FOR PLAYERS..." to a running match within a couple of seconds — no menus, no configuration. Player slots, spawn sides, and colors (green/red) are negotiated automatically.

**Two machines on a LAN:** run one instance per machine; discovery uses UDP broadcast on port 3000. Options:

| Argument | Effect |
|---|---|
| `-DISPort=3000` | UDP port (default 3000) |
| `-DISBroadcastAddress=192.168.1.255` | Broadcast address (default 255.255.255.255) |
| `-AutoPilot` | Tank drives and fires itself (testing) |
| `-NetSimLatencyMs= -NetSimJitterMs= -NetSimLossPct= -NetSimDupPct= -NetSimSeed=` | Inbound network condition simulation |

A single manual launch looks like:

```powershell
& "<UE_5.7>\Engine\Binaries\Win64\UnrealEditor.exe" "<path>\DISMultiplayerTank.uproject" -game -windowed -resx=960 -resy=540 -log
```

### Controls

| Key | Action |
|---|---|
| W / S (or Up / Down) | Drive forward / reverse |
| A / D (or Left / Right) | Turn — also steers your shell in flight |
| Space | Fire (one shell at a time) |

First to **10 kills** wins the round; the map and scores reset automatically.

## Network integration tests

Semi-automated lag-condition testing (two autopilot instances fight, logs are parsed and cross-asserted):

```powershell
.\Tools\RunNetTest.ps1 -Profile Baseline   # also: Latency | Loss | Nasty
```

Asserts per run: both instances start a match, no fatal errors, kill/score views converge across instances, and dead-reckoning error stays bounded. Exit code 0 = pass. Results for all four profiles are recorded in the technical notes.

## Project layout

```
Source/DISMultiplayerTank/       C++ module (Public/Private split)
  Core/        GameMode, PlayerController, procedural Arena
  Tanks/       Tank pawn (owned + ghost modes)
  Shells/      Steerable shell (owned + ghost modes)
  Networking/  DIS integration: PDU router, peer registry, net conditioner
  Testing/     AutoPilot driver
  UI/          Canvas HUD
Tools/                           Launch and test scripts
Docs/                            Architecture plan and technical notes
```
