# Porting plan and first technical audit

## Objective

Run the modern Team Fortress 2 game on Meta Quest 3 as a standalone Android application with native OpenXR VR. Preserve the original game's class, weapon, map, movement, and multiplayer systems rather than recreating them in another engine.

## Reality check

Valve's public Source SDK 2013 repository includes TF2 client and server game code. It is not a turnkey Android port of the complete Source engine. The published build instructions describe Windows and Linux/Steam Runtime workflows. Android ARM64 and OpenXR support must be engineered and validated separately.

## Architecture workstreams

### 1. Source and dependency audit
- Pin the exact Valve upstream commit used for each experiment.
- Identify engine modules required by TF2 client: filesystem/VPK, materials, model loading, renderer, audio, input, networking, UI, Steam/platform interfaces.
- Separate game code from desktop-only platform code.
- Record licenses and redistribution constraints for every dependency.
- Determine which services can run on-device and which require a compatible external server/service.

### 2. Android platform layer
- Android Gradle application using NDK C++ and ABI `arm64-v8a`.
- Native activity lifecycle, storage/content access, logging, crash diagnostics.
- OpenXR Android loader and Quest runtime session lifecycle.
- Vulkan graphics path and stereo swapchains.
- Audio output and controller/headset input.

### 3. First device milestone
The first APK should only prove that the Quest app can:
1. launch and create an OpenXR session;
2. render a stable stereo test scene;
3. track head pose;
4. read left/right Touch controller poses and buttons;
5. exit and recover cleanly through Android lifecycle events.

This is a platform proof, not yet TF2 gameplay.

### 4. Engine integration
- Port or adapt the minimum engine/platform dependencies incrementally.
- Keep upstream changes isolated and documented.
- Verify asset loading with user-provided, legitimately obtained TF2 content.
- Avoid bundling proprietary game files in this repository.

### 5. VR gameplay
- Head-directed view with configurable comfort options.
- Tracked hands and weapon poses; trigger/grip mappings.
- VR locomotion and snap/smooth turning options.
- Diegetic and/or wrist/menu HUD interaction without breaking original game UI.
- Multiplayer synchronization and server compatibility testing.

## Main risks / blockers

- Source engine desktop assumptions and platform-specific code.
- Renderer/material/shader compatibility with Android Vulkan and mobile GPU limits.
- Steam APIs and platform services are not automatically available as Android equivalents.
- Memory, CPU/GPU frame time, thermal limits, and loading times on Quest 3.
- Multiplayer anti-cheat, matchmaking, authentication, and server compatibility.
- Valve SDK license is non-commercial; content and third-party licenses must also be respected.

## Definition of done for milestone 1

A documented dependency map with exact upstream revision, identified build blockers, and a proposed Android/OpenXR integration boundary. Do not mark the milestone complete based on a desktop build or a mock scene.

## Immediate next actions

1. Clone Valve's upstream repository and inspect its TF2 game and engine directory boundaries.
2. Record the upstream commit SHA and license notices.
3. Inventory Linux/Windows-only dependencies and identify Android replacements or porting requirements.
4. Create the Android OpenXR proof-of-life app in a separate project layer.
5. Build and test on physical Quest 3 before starting full engine integration.
