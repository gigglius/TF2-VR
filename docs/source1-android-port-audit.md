# Source 1 / TF2 Android ARM64 port audit

## What we have

The official Valve repository is pinned as a Git submodule at:

- Repository: https://github.com/ValveSoftware/source-sdk-2013
- Revision: `e1f1fd42271805072f052051c26c21caa2c87b04`
- Source tree: TF2 client and server game code plus Source SDK 2013 game-code infrastructure.

Valve's README documents Windows builds using Visual Studio 2022 and Linux builds against the Steam Runtime. Those are not Android/NDK build instructions.

## Critical boundary

The published SDK is not a complete portable Source 1 engine. TF2's game code expects engine interfaces and libraries for rendering, materials, model loading, physics, audio, input, filesystem/VPK access, networking, matchmaking/platform services, and other systems. The current Quest app implements only an Android NativeActivity + OpenXR/Vulkan presentation shell. It cannot load the TF2 client yet.

Therefore, simply adding the TF2 game-code directory to the Android Gradle build will not produce a playable port. We must first identify the required engine components and determine which are available to build from source, which are supplied only as desktop binaries, and which require Android implementations.

## Port strategy

1. Keep Valve's source as a pinned submodule; do not copy or silently modify upstream code.
2. Inventory TF2 client targets, engine interfaces, platform-specific code, third-party libraries, and desktop-only dependencies.
3. Establish an Android ARM64 engine target. Replace Windows/Linux/Steam Runtime assumptions with Android/NDK equivalents where source is available.
4. Bring up the minimum engine services in stages: filesystem and VPK access, materials/model loading, jobs/threads, input, audio, physics, networking, and renderer.
5. Replace the desktop renderer with a Vulkan path that renders into the OpenXR eye swapchains already owned by the Quest app.
6. Link the TF2 client/game code only after its engine dependencies are satisfied; then launch a minimal map before adding VR-specific gameplay.
7. Keep Steam-dependent inventory, matchmaking, and authentication work separate from local/offline engine bring-up.

## Immediate next engineering task

Run a reproducible source audit against the pinned revision and produce a dependency map for the TF2 client build. The first real engine milestone is not a menu or a mock scene: it is compiling a small, isolated set of Source engine/game-code dependencies for Android ARM64 and documenting the first blockers.

## Licensing and content

Valve's Source 1 SDK license is non-commercial. Distributions must follow the license conditions and include the required license and third-party notices. Keep proprietary TF2 assets and binaries out of this repository; users must supply legally obtained game content. This audit is technical documentation, not legal advice.
