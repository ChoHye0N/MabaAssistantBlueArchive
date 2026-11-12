# GUI (C#, WPF)

`Maba.Gui/`: WPF app on .NET 10 with HandyControls (theme and controls) and CommunityToolkit.Mvvm (MVVM). It talks to Core only through the C ABI in `core/include/maba/maba_api.h` (`Native/NativeMethods.cs`, `Native/CoreClient.cs`).

## Build
1. Build Core first: `cmake --preset windows-msvc`, `cmake --build --preset windows-msvc`.
2. Install the .NET 10 SDK, then `dotnet build gui/Maba.Gui -c Release`. `MabaCore.dll` and its runtime DLLs (OpenCV) are copied from `build/windows-msvc/core/Release/`; pass `-p:MabaCoreDir=<dir>\` when Core was built elsewhere.
3. Run `gui/Maba.Gui/bin/Release/net10.0-windows/MabaAssistantBlueArchive.exe`. It uses its own data folder (`pages/`, `tasks/`, `config/`): `data/` beside the executable in a release, otherwise the first folder found upward from the executable (the repository). The folder in the Settings tab is used only when neither exists, because the settings file is shared by every copy of the program on the PC.

## Layout
- `Views/MainWindow.xaml`: tabs for tasks, my info, settings and log. The tasks tab lists the tasks on the left (drag the grip to reorder) and shows the chosen task (description, settings, last result) or a summary of the day on the right.
- `ViewModels/MainViewModel.cs`: task selection, connect/start/stop, Core events, the daily scheduler.
- `Services/SettingsStore.cs`: settings in `%APPDATA%\MabaAssistantBlueArchive\settings.json` (user profile, never the repository).
- `ViewModels/AccountCard.cs`: one box per `accounts/<uid>.json`.
- Task profiles (`TaskProfile` in `Services/SettingsStore.cs`): selected tasks, their settings and their order; the run passes the order to Core (`"ordered": true`), which still runs `after` dependencies first.
- `Views/ConfirmDialog.xaml`: questions in the window's own style, without the system sound of the library's message box.
- `Services/UpdateChecker.cs`: the new-version notice; only in builds that got a repository name (`MabaRepository`).
- `Resources/Strings.<lang>.xaml`: UI text; task display names use `Task.<task id>`.

## Notes
The callback runs on a worker thread and must not call `MabaStart` or `MabaDestroy`. Behaviour: `docs/ARCHITECTURE.md`, "GUI".
