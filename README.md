# NoUbiNoCry

[![Build](https://github.com/RGB-Outl4w/NoUbiNoCry/actions/workflows/build.yml/badge.svg)](https://github.com/RGB-Outl4w/NoUbiNoCry/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/RGB-Outl4w/NoUbiNoCry)](https://github.com/RGB-Outl4w/NoUbiNoCry/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Removes the **"Connecting to Ubisoft servers"** popup and the loading hang from **Far Cry 3: Blood Dragon** on PC.

Ubisoft shut down the game's online backend, but the game still tries to log in on startup and every time you pause. Each attempt waits for a network timeout (from 15 seconds to over a minute) before the game gives up. NoUbiNoCry is a tiny drop-in DLL that makes those attempts fail instantly, so the game carries on offline, as it was designed to when no connection is available.

## Features

- Pause menu opens instantly, with no "Connecting…" popup.
- Faster startup to the main menu.
- No game files are modified. Uninstalling means deleting one file.
- Works with both the DX9 and DX11 executables.
- Saves and achievements are untouched; they go through Ubisoft Connect, not the dead servers.
- Not a crack: Ubisoft Connect still launches and checks ownership as usual.
- Open source, about 250 lines of C.

## Installation

1. Download `NoUbiNoCry-vX.Y.Z.zip` from the [latest release](https://github.com/RGB-Outl4w/NoUbiNoCry/releases/latest).
2. Extract `dinput8.dll` into the game's `bin` folder, next to `fc3_blooddragon.exe`:

   | Store           | Folder                                                      |
   | --------------- | ----------------------------------------------------------- |
   | Steam           | `...\steamapps\common\Far Cry 3 Blood Dragon\bin\`          |
   | Ubisoft Connect | `...\Ubisoft Game Launcher\games\Far Cry 3 Blood Dragon\bin\` |

3. Start the game as usual. No launch options are needed.

**Uninstall:** delete `bin\dinput8.dll` and `bin\NoUbiNoCry.log`.

## Compatibility

| Version                          | Status                                                    |
| -------------------------------- | --------------------------------------------------------- |
| Steam, DX11 (build 7796294)      | Tested in-game                                            |
| Steam, DX9                       | Patch verified by the self-test; not yet tested in-game   |
| Ubisoft Connect                  | Untested; please [report](../../issues) how it goes       |

- Mods that also ship a `dinput8.dll` (e.g. ASI loaders) can't be installed alongside it, because only one file can have that name.
- The community "Service Not Available Fix" (patched `FC3.dll` / `FC3_d3d11.dll`) solves the same problem, so you don't need both.

## Troubleshooting

Each launch writes `bin\NoUbiNoCry.log`. A working install starts like this:

```text
NoUbiNoCry 1.0.0 | FC3_d3d11.dll: network cut off (5 winsock imports redirected, 5 were already bound)
[  11.172s] blocked lookup onlineconfigservice.ubi.com
```

- **No log file:** the DLL wasn't loaded. Check that it's in `bin`, next to the game's `.exe`.
- **Fewer than 5 imports, or `0 were already bound`:** please [open an issue](../../issues) and attach the log.

## How it works

Far Cry 3: Blood Dragon runs on the Dunia 2 engine. Its online features use Quazal RendezVous:
1. The login service fetches a config from `onlineconfigservice.ubi.com`.
2. It then connects to an RDV server (e.g. `pdc-lb-rdv-prod02.ubisoft.com`).
3. Those hosts still resolve, but the login never completes, so every attempt sits in a handshake timeout. While that runs, the game shows the "Connecting" popup.

NoUbiNoCry is a `dinput8.dll` proxy:

1. **Loading.** Both game DLLs import `DirectInput8Create`, and Windows loads `dinput8.dll` from the executable's folder first. The proxy forwards that call to the real system DLL.
2. **Patching.** On load, it rewrites the import table of the game module only (`FC3.dll` or `FC3_d3d11.dll`). The entries for `getaddrinfo`, `gethostbyname`, `connect`, `sendto` and `WSASendTo` are pointed at stubs. The stubs allow local and loopback addresses and fail everything else immediately (`WSAHOST_NOT_FOUND` / `WSAENETUNREACH`).
3. **Result.** The game's own, unmodified login code gets "host not found" within milliseconds and moves on as offline. Ubisoft Connect, Steam and the overlays use their own DLLs, so they are not affected.

## Recommended settings (optional)

**Cap the frame rate.** The engine renders uncapped by default, which keeps the CPU busy and hot.
- **NVIDIA:** Control Panel → Manage 3D settings → Program Settings. Add `fc3_blooddragon_d3d11.exe` (or `fc3_blooddragon.exe` for DX9) and set **Max Frame Rate** to e.g. 90–120.
- **AMD:** Adrenalin → the game's profile → Frame Rate Target Control or Radeon Chill.
- **Fullscreen, no tools:** set `RefreshRate="120"` (any mode your monitor supports) and keep `VSync="1"` in `GamerProfile.xml`.

**Skip the intro videos:** add `-skipintro` to the game's launch options.

**Profile tweaks.** Edit `Documents\My Games\Far Cry 3 Blood Dragon\GamerProfile.xml` with the game closed:

| Setting                          | Effect                                                        |
| -------------------------------- | ------------------------------------------------------------- |
| `Fullscreen="0" Borderless="1"`  | Borderless window                                             |
| `FOVScaleFactor="1.4"`           | Wider FOV than the slider allows (moving the slider resets it) |
| `Smoothness_Ironsight="0"`       | Raw iron-sight aim (keep `UseMouseSmooth="0"`)                |

## Building from source

Requirements: Visual Studio 2022 or newer, with the **Desktop development with C++** workload.

```bat
build.cmd            :: builds dinput8.dll and selftest.exe
build.cmd package    :: also creates dist\NoUbiNoCry-v<version>.zip
```

The self-test maps a game DLL without running it, applies the patch, and checks the blocking rules. Exit code 0 means OK:

```bat
selftest.exe "<game>\bin\FC3_d3d11.dll"
```

| File             | Purpose                                                          |
| ---------------- | ---------------------------------------------------------------- |
| `noubinocry.c`   | Proxy, import patching, network stubs, log and self-test          |
| `noubinocry.rc`  | Version resource (shown in the DLL's Properties → Details)       |
| `version.h`      | The single place the version number is defined                   |
| `build.cmd`      | Build and packaging script                                       |

**Releasing a new version:**
1. Bump `version.h`.
2. Add a `CHANGELOG.md` entry.
3. Push a tag `vX.Y.Z`. CI then builds the zip and publishes the GitHub release.

## License

[MIT](LICENSE) © 2026 OutlawRGB

## Disclaimer

This project is not affiliated with or endorsed by Ubisoft. Far Cry is a trademark of Ubisoft Entertainment.

NoUbiNoCry does not modify game files, bypass Ubisoft Connect, or unlock content. Use at your own risk.
