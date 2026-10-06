# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-10-06

First release.

### Added
- `dinput8.dll` proxy that cuts the game's own network access, so its attempts to reach Ubisoft's shut-down RendezVous servers fail instantly. This removes:
  - the long load before the main menu
  - the "Connecting to Ubisoft servers" popup and hang on the pause menu
- Covers both executables: DX9 (`FC3.dll`) and DX11 (`FC3_d3d11.dll`).
- Only the game module is cut off. Ubisoft Connect (saves, achievements) and Steam are untouched.
- `NoUbiNoCry.log` written next to the DLL, recording the patch result and every blocked lookup or connection.

[1.0.0]: https://github.com/RGB-Outl4w/NoUbiNoCry/releases/tag/v1.0.0
