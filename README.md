# TF2-VR — Team Fortress 2 for Meta Quest 3

A research project to bring the real Team Fortress 2 game to standalone Quest 3 using Android ARM64, Vulkan, and OpenXR. This is an engine-port effort, not a Unity remake.

## Current status

The Quest 3 app now builds in GitHub Actions and has been installed and tested on a Quest 3. The user has confirmed the stereo calibration view, head-tracked view, and a brief controller haptic confirmation. The current APK is still a platform test: it does **not** run TF2 gameplay yet.

## Valve source integration

- `third_party/source-sdk-2013` is a pinned Git submodule of Valve's official Source SDK 2013 repository, at upstream commit `e1f1fd42271805072f052051c26c21caa2c87b04`.
- Valve added TF2 client and server game code to the SDK in February 2025.
- This SDK is game code on top of Source 1; it is **not the complete Source engine**. The engine runtime, renderer, physics, platform layer and prebuilt dependencies are not all supplied as a portable Android implementation.
- The current Android app is an OpenXR/Vulkan shell and has not yet compiled or linked the Source engine or TF2 client.

See [the Source 1 Android port audit](docs/source1-android-port-audit.md) for the integration boundary and next engineering milestones.

## Target

- Device: Meta Quest 3 standalone
- OS / ABI: Android, arm64-v8a
- XR: OpenXR
- Graphics: Vulkan stereo rendering
- Input: Touch controller tracking, VR locomotion, weapon interaction and haptics
- Game: modern TF2 code and compatible game content, subject to Valve's SDK license and game terms

## Development milestones

1. Quest OpenXR/Vulkan shell and controller/haptic verification — **working prototype**
2. Pin and audit Valve's TF2 game code — **source pinned; audit in progress**
3. Port/build the missing Source 1 engine and platform dependencies for Android ARM64
4. Link and launch the TF2 client in a basic Quest VR scene
5. Implement VR hands, weapons, locomotion, HUD and comfort options
6. Validate multiplayer/server compatibility and optimize for Quest 3
7. Produce and test a distributable standalone APK

## Repository layout

- `android/` — Android ARM64 NativeActivity and OpenXR/Vulkan platform layer
- `third_party/source-sdk-2013/` — pinned Valve SDK submodule
- `docs/` — architecture and porting notes
- `.github/workflows/` — cloud Android builds

## Legal and content policy

The Source 1 SDK license is non-commercial and requires compliance with its distribution terms. Any mod/build using the SDK must be free. Do not commit proprietary TF2 game assets, Steam credentials, or copyrighted game files. Use content obtained through legitimate channels. This repository does not grant rights to TF2 assets or trademarks.
