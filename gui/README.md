# GUI (C#, WPF) - planned

Not scaffolded yet: WPF cannot be built in the cloud Linux workspace. Create it on Windows:
`dotnet new wpf -n Maba.Gui -f net8.0` then set `TargetFramework` to `net8.0-windows`.

Plan:
- Talks to Core only through the C ABI in `core/include/maba/maba_api.h` (P/Invoke, cdecl).
- MVVM.
- Optional theme: ModernWpfUI (MIT, as used in the reference project; verify when adding).
- Add each dependency to the README table and `THIRD_PARTY_NOTICES.md`.
