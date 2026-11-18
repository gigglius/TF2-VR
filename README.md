# TF2-VR — Team Fortress 2 for Meta Quest 3

A research project to bring the real Team Fortress 2 game to standalone Quest 3 using Android ARM64, Vulkan, and OpenXR. This is an engine-port effort, not a Unity remake.

## Current status

The Quest 3 OpenXR/Vulkan platform-test APK builds in GitHub Actions. The app has previously been installed and tested on a Quest 3, including head tracking and a brief controller haptic confirmation. The current APK does **not** run TF2 gameplay.

## Valve source integration

- `third_party/source-sdk-2013` is pinned to Valve's official Source SDK 2013 repository at `e1f1fd42271805072f052051c26c21caa2c87b04`.
- The official SDK includes TF2 client/server game code and shared SDK code.
- A recursive inspection of the pinned tree found only `src/engine/audio/public/sound.h` under `src/engine` and zero engine implementation `.cpp` files. The complete Source 1 runtime is therefore **not present** in this checkout.
- The Android app is currently an OpenXR/Vulkan shell; it has not compiled or linked the Source engine or TF2 client.

See [the Source 1 Android port audit](docs/source1-android-port-audit.md) for the verified blocker and legitimate next steps.

## Target

- Device: Meta Quest 3 standalone
- OS / ABI: Android, arm64-v8a
- XR: OpenXR
- Graphics: Vulkan stereo rendering
- Game: actual TF2 client/game code, subject to Valve's SDK license and game terms

## Milestones

1. Quest OpenXR/Vulkan shell — **builds; prior headset testing reported**
2. Pin and audit Valve TF2 game code — **complete**
3. Obtain an authorized complete Source 1 engine runtime — **blocked / required**
4. Port and build that runtime for Android ARM64
5. Link and launch the TF2 client in a basic Quest VR scene
6. Implement VR hands, weapons, locomotion, HUD and comfort options
7. Validate multiplayer/server compatibility and optimize for Quest 3
8. Produce and test a distributable standalone APK

## Repository layout

- `android/` — Android ARM64 NativeActivity and OpenXR/Vulkan platform layer
- `third_party/source-sdk-2013/` — pinned Valve SDK submodule
- `docs/` — architecture and porting notes
- `.github/workflows/` — cloud Android builds

## Legal and content policy

The Source 1 SDK license is non-commercial and has distribution conditions. Do not commit proprietary TF2 game assets, Steam credentials, leaked source or copyrighted game files. Users must supply legally obtained TF2 content. This repository does not grant rights to TF2 assets or trademarks.
