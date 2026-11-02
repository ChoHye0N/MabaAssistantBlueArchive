# GUI (C#, WPF)

`Maba.Gui/`: WPF app on .NET 10 with HandyControls (theme and controls) and CommunityToolkit.Mvvm (MVVM). It talks to Core only through the C ABI in `core/include/maba/maba_api.h` (`Native/NativeMethods.cs`, `Native/CoreClient.cs`).

## Build
1. Build Core first: `cmake --preset windows-msvc`, `cmake --build --preset windows-msvc`.
2. Install the .NET 10 SDK, then `dotnet build gui/Maba.Gui -c Release`. `MabaCore.dll` and its runtime DLLs (OpenCV) are copied from `build/windows-msvc/core/Release/`; pass `-p:MabaCoreDir=<dir>\` when Core was built elsewhere.
3. Run `gui/Maba.Gui/bin/Release/net10.0-windows/MabaAssistantBlueArchive.exe`. It finds the data folder (`pages/`, `tasks/`, `config/`) by searching upward from the executable; the Settings tab can point it elsewhere.

## Layout
- `Views/MainWindow.xaml`: tabs for tasks, settings and log.
- `ViewModels/MainViewModel.cs`: task selection, connect/start/stop, Core events, the daily scheduler.
- `Services/SettingsStore.cs`: settings in `%APPDATA%\MabaAssistantBlueArchive\settings.json` (user profile, never the repository).
- `Resources/Strings.ko.xaml`: UI text (Korean baseline). Another language gets `Strings.<lang>.xaml` with the same keys; task display names use `Task.<task id>`.

## Notes
- Core events arrive on a worker thread and are marshalled to the UI thread. The callback must not call `MabaStart` or `MabaDestroy`.
- The scheduler checks every 30 s whether today's slot (server reset + delay, computed by Core) is due. A scheduled day counts as done even if tasks failed, so a broken task never loops.
- Add each new NuGet package to the README tables (both languages) and `THIRD_PARTY_NOTICES.md`.
