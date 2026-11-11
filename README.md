<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->

<p align="center">
  <img src="dist/branding/lichii_icon_with_logo.png" alt="Lichii" width="600">
</p>

# OOT Remake

A build of Frizti's Fritchii and zortos293's Lichii (see Credits) dedicated to The Legend
of Zelda: Ocarina of Time on Switch 2 (program id
0400486024DEC000). Launched without arguments, `OOT Remake.exe` opens a home screen for the
game: its icon, title and version read from your dump (NSP/XCI/NUP or the exefs `main`, asked
once), a Play button and quick settings (frame rate, resolution, fullscreen, anti-aliasing,
volume, controller). The built-in Max Resolution and Dynamic FPS
patches are applied to build E0F3CCFAF50065AE4EA01EB5AEB13B1A45DA3348. Keys, firmware and the
game are not included: use your own dump.

The build is always portable: all data (config, keys, firmware/NAND, saves, shader cache, logs)
lives in the `portable` folder next to `OOT Remake.exe`, created on first launch. Put
`prod.keys` and `title.keys` in `portable\keys`.

Windows needs the Microsoft Visual C++ 2015-2022 Redistributable (x64). It is not bundled: if
`OOT Remake.exe` reports a missing `MSVCP140.dll` or `VCRUNTIME140.dll`, install it from
https://aka.ms/vs/17/release/vc_redist.x64.exe. Windows builds come from the `oot-windows` workflow
(Actions tab, artifact `OOT-Remake-windows-*`).

For the "Preparing shaders" screen, copy the `recipes` folder of a Fritchii release (same cache
version) next to `OOT Remake.exe`.

## Credits

OOT Remake (Djipi) is built on the work of others; thank you to all of them:

- **[Fritchii](https://github.com/Frizti/Fritchii) by Frizti**: the base of this build. The
  Ocarina of Time engine work (the "Preparing shaders" screen and precompiled pipelines, async
  shader translation, render target relocation and image pool, the render-frame clock, the host
  frame pacer, the high refresh mode, the tuned settings, and many fixes and performance gains).
- **[Lichii](https://github.com/zortos293/Lichii) by zortos293 and its contributors**: the
  Switch 2 emulation Fritchii builds on, and the fixes ported here from Lichii up to build-150
  (damaged NUP dumps #1197 by RsMan-Dev, the ocarina crackle #1196, and others named in the
  commit history).
- Eglantine, Eden, yuzu and their contributors, whose work Lichii builds on.
- Dark's Dynamic FPS, bundled with his permission by Lichii and Fritchii.

All of them are GPL-3.0 (yuzu parts GPL-2.0-or-later); the source keeps their copyright notices.
OOT Remake is not affiliated with Nintendo.

Everything below is the upstream Lichii README.

# Lichii

Lichii is an experimental, open-source Nintendo Switch and Switch 2 emulator written in C++. It builds on Eglantine, Eden, yuzu and Sudachi, preserving their contributions while developing this fork's game compatibility and renderer fixes.

This is an independent fork, not an official release of those upstream projects. Lichii is not affiliated with Nintendo.

## Downloads and setup

Published builds belong on this fork's [releases page](https://github.com/zortos293/Eglantine/releases). The repository still uses its historical Eglantine name; the desktop application is now Lichii.

On Windows, extract the entire build archive and run **lichii.exe**. Only use games and system files you have legally obtained. Games, keys and firmware are not included.

### Existing saves and settings

The rebrand intentionally retains the existing Eden/Eglantine data directories and configuration keys. It does not move or delete saves, settings or shader caches. Back up your saves before testing experimental builds.

## Compatibility and issue reports

Compatibility and performance vary by game, hardware and driver. Rendering errors, stutters and crashes remain possible. Passing automated tests does not establish that a game renders correctly, and this project makes no universal frame-rate guarantee.

When [reporting an issue](https://github.com/zortos293/Eglantine/issues), include:

- The exact build, game version, CPU, GPU and driver version.
- Relevant settings and steps to reproduce the problem.
- A screenshot and the log from the affected session, after checking it for personal information.

Do not upload game files, keys, firmware or private account information.

## Building and contributing

See the [build guide](docs/Build.md), [user documentation](docs/user/README.md) and [coding guidelines](docs/Coding.md). Some inherited documentation and internal build target names still use upstream names. The GUI build target remains `yuzu`; its output is `lichii.exe` on Windows and `lichii` on other desktop platforms.

Keep changes focused and distinguish code-level checks from actual gameplay testing.

## Artwork
Art created by YOB (yobslab)
The supplied Lichii logo, icon and library artwork are in [dist/branding](dist/branding). Windows, Linux and macOS icon resources are generated from that artwork by [Generate-LichiiIcons.ps1](tools/Generate-LichiiIcons.ps1).

## Support Lichii

Lichii is free and stays free. If you want to help, you can donate in crypto. Donations help pay for the AI tools and build machines used to develop this open-source project faster.

**Solana (SOL)**
```text
JepEjfyWjGjhJvHSrqj52KtLjcbbbmUh1iGqerYBF4w
```

**Ethereum (ETH)**
```text
0x055ABa1b80Bc35C836Ce7E9F47c2b3d6C75b5534
```

**Bitcoin (BTC)**
```text
bc1qcvxdpf4qvdhqp8ee4s355cjmxrzsphsdwpl8w4
```

**BNB Smart Chain (BNB)**
```text
0x055ABa1b80Bc35C836Ce7E9F47c2b3d6C75b5534
```

**Polygon**
```text
0x055ABa1b80Bc35C836Ce7E9F47c2b3d6C75b5534
```

Send each coin on its matching network. Coins sent on the wrong network can't be recovered.

> [!NOTE]
> Donations are voluntary gifts and are non-refundable. A donation does not buy any product, service, perk, early access, exclusive build, priority support or say in the project. Every release stays free and public for everyone.

## Credits and license

Lichii inherits the work of the Eglantine, Eden, yuzu and Sudachi projects and their contributors, including Eglantine's original developer Camille LaVey. Original copyright notices and third-party licenses are retained.

See [LICENSE.txt](LICENSE.txt) and the source files for applicable license terms.
