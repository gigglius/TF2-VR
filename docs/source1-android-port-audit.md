# Source 1 / TF2 Android ARM64 engine-port audit

## Verified repository state

The project pins Valve's official Source SDK 2013 repository as a submodule:

- Upstream: https://github.com/ValveSoftware/source-sdk-2013
- Pinned revision: `e1f1fd42271805072f052051c26c21caa2c87b04`
- TF2 client and server project definitions: present
- TF2 game/client and shared game code: present

A recursive Git tree inspection of that exact revision returned 7,780 paths. Under `src/engine`, it found exactly one file:

```
src/engine/audio/public/sound.h
```

There are **zero engine implementation .cpp files** in `src/engine`. The public SDK therefore does not provide the complete Source 1 runtime needed to launch TF2. Other SDK directories contain shared libraries and interfaces, but those do not replace the missing engine implementation and platform runtime.

## Current Quest implementation

The Android project is an ARM64 NativeActivity + OpenXR/Vulkan shell. It is not linked to the TF2 client or a Source engine runtime. Its successful APK build verifies the Android/XR shell only.

## Why the next engine build is blocked

The TF2 game code expects a compatible Source 1 engine runtime and platform services, including engine interfaces, client/server runtime, rendering/material systems, model and filesystem/VPK loading, physics, audio, threading/jobs, networking, and platform integration.

The current public SDK checkout does not include the full engine implementation. Adding the TF2 client VPC project to Gradle or linking the existing Android OpenXR loader cannot create those missing systems.

## Legitimate path forward

1. Keep Valve's SDK submodule pinned and unmodified.
2. Request written guidance or authorized engine source access from Valve. Valve's published guidance says source access beyond the SDK has historically been exceptional; contact Valve through its official support channels. See https://partner.steamgames.com/doc/sdk/uploading/distributing_source_engine.
3. Do not use leaked source dumps, leaked binaries, or unofficial repositories claiming to contain leaked TF2/Source engine code.
4. Once an authorized engine runtime/source path is established, inventory its build targets and desktop dependencies.
5. Create an Android NDK ARM64 platform target for that runtime, replacing desktop OS, window, input, audio, filesystem, threading and graphics assumptions.
6. Integrate the engine renderer with the existing OpenXR Vulkan eye swapchains.
7. Build the TF2 client against the compatible runtime and first launch a minimal locally supplied map.
8. Add VR interaction, hands, weapons, locomotion, HUD, then investigate multiplayer and Steam-dependent services.

## Immediate milestone

**Blocked pending an authorized complete Source 1 runtime.** The automated audit now reports the exact missing implementation instead of treating the presence of an `src/engine` directory as proof that the engine is available.

## Licensing and content

Valve's Source 1 SDK license is non-commercial and imposes distribution conditions. Valve's Steamworks guidance also says TF2 mods may not redistribute TF2 game files; users must supply TF2 content from a legitimate installation. This technical note is not legal advice. See:

- https://github.com/ValveSoftware/source-sdk-2013/blob/master/LICENSE
- https://partner.steamgames.com/doc/sdk/uploading/distributing_source_engine
