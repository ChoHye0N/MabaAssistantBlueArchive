[한국어](README.md) | **English**

<div align="center">

# MABA

**MabaAssistantBlueArchive**

Runs your Blue Archive dailies for you

![license](https://img.shields.io/badge/license-AGPL--3.0--only-blue)
![platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![status](https://img.shields.io/badge/status-beta-orange)

</div>

<div align="center">
  <img src="docs/images/screenshot-ko.png" alt="MABA window" width="720">
</div>

## Overview

From the cafe to the mission rewards, MABA works through the daily routine in a fixed order. It looks at the screen and moves the mouse; it never touches game files or memory.

**Supported** · Windows 10/11 · global server (Steam PC client, Korean first)
Android emulators over ADB can be connected too, but are not verified yet.

[Download from the releases page](../../releases)

## What it does

In run order. Each task can be switched on or off, and its details are set in the settings.

**Collect**: cafe income · student invitations · student taps · schedule · circle attendance · daily free package

**Spend**: shop bulk purchase · bounty · AP (event story and quests, campaign missions, scrimmage) · tactical challenge

**Finish**: mailbox · remaining AP · mission rewards · resource record per account

Also: scheduled runs keyed to the server reset, automatic handling of login notices and reward screens, AUTO and 3x speed kept on in battles.

## Install

1. Download the archive from the releases page and unpack it anywhere.
2. Install the [.NET 10 Desktop Runtime](https://dotnet.microsoft.com/download/dotnet/10.0) if you do not have it.
3. Prepare the templates (small pieces of game screens). Game images are not distributed for copyright reasons: make them with the [template capture tool](tools/README.md) and put them in `assets-local/`.
4. Tasks that read numbers (schedule, AP, ...) need OCR. Follow [OCR setup](ocr/README.md) to create the Python environment.
5. Run `MabaAssistantBlueArchive.exe`.

Settings are stored in `%appdata%\MabaAssistantBlueArchive`.

## Tips

> [!TIP]
> Run the game in **windowed mode (16:9)** and leave the mouse alone while MABA works. If another window covers the game, MABA stops instead of clicking.

> [!WARNING]
> If the game runs as administrator, Windows blocks the input. Run the game and MABA with the same rights.

- Set up the team (1-4) used for quests that have to be played in the game beforehand.
- Write students to invite exactly as the game shows their names.

## Development

**Core** is C++20 (OpenCV), the **GUI** is C# WPF (.NET 10), and **OCR** runs in a Python sidecar (RapidOCR). Tasks are data in `tasks/*.json`, so changing the order or the steps needs no code change.

```
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
dotnet build gui/Maba.Gui -c Release
```

The Windows build needs Visual Studio 2022 and `VCPKG_ROOT`. On Linux the `linux-dev` preset builds and tests the core logic.

Further reading: [architecture](docs/ARCHITECTURE.md) · [task format](docs/PAGE_FORMAT.md) · [roadmap](docs/ROADMAP.md) · [decisions](docs/DECISIONS.md)

<details>
<summary><b>Open source used</b></summary>

| Name | Version | License | Use |
|---|---|---|---|
| OpenCV | 4.12.0 | Apache-2.0 | template matching, image I/O |
| nlohmann-json | 3.12.0 | MIT | reading data files |
| HandyControls | 3.7.0 | MIT | GUI theme and controls |
| CommunityToolkit.Mvvm | 8.4.2 | MIT | GUI MVVM |
| RapidOCR | 3.10.0 | Apache-2.0 | OCR engine |
| ONNX Runtime | 1.31.1 | MIT | OCR model inference |
| PaddleOCR models | downloaded by RapidOCR | Apache-2.0 | text detection and recognition |
| Pillow | 12.3.0 | MIT-CMU | template capture tool |
| MaaTouch | supplied by the user | Apache-2.0 | emulator touch (optional, not included) |

Full notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

</details>

## Contributing

Please read the [contributing guide](CONTRIBUTING.md) first: it covers the commit format and the privacy check.

## License

The code is [AGPL-3.0-only](LICENSE).

Blue Archive and its images belong to NEXON Korea Corp. and NEXON GAMES Co., Ltd. and are not covered by this license. This project is not affiliated with or endorsed by NEXON. Using automation may break the terms of service; any consequence, including account penalties, is your own responsibility.
