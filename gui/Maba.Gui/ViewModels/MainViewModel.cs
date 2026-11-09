// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Text.Json.Nodes;
using System.Windows.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Maba.Gui.Native;
using Maba.Gui.Services;

namespace Maba.Gui.ViewModels;

public sealed record LogEntry(DateTime Time, string Level, string Message)
{
    public string TimeText => Time.ToString("HH:mm:ss");
}

public sealed record Choice(string Value, string Label);

public partial class MainViewModel : ObservableObject, IDisposable
{
    private const int MaxLogEntries = 2000;

    private readonly AppSettings _settings;
    private readonly SettingsStore _store;
    private readonly Dispatcher _dispatcher;
    private readonly DispatcherTimer _scheduleTimer;
    private CoreClient? _core;
    private bool _connected;
    private string _connectedWith = "";
    private bool _runIsScheduled;
    private bool _initializing = true;

    public ObservableCollection<AccountCard> Accounts { get; } = new();
    public bool HasAccounts => Accounts.Count > 0;

    public MainViewModel(AppSettings settings, SettingsStore store, Dispatcher dispatcher)
    {
        _settings = settings;
        _store = store;
        _dispatcher = dispatcher;
        _controller = settings.Controller;
        _capture = settings.Capture;
        _fitWindow = settings.FitWindow;
        _adbPath = settings.AdbPath;
        _adbSerial = settings.AdbSerial;
        _adbScreencap = settings.AdbScreencap;
        _adbTouch = settings.AdbTouch;
        _maaTouchPath = settings.MaaTouchPath;
        _backKeycode = settings.BackKeycode;
        _startEmulator = settings.StartEmulator;
        _emulatorPath = settings.EmulatorPath;
        _emulatorArgs = settings.EmulatorArgs;
        _startGame = settings.StartGame;
        _closeGameAfter = settings.CloseGameAfter;
        _closeEmulatorAfter = settings.CloseEmulatorAfter;
        _steamStartGame = settings.SteamStartGame;
        _steamCloseGame = settings.SteamCloseGame;
        _ocrEnabled = settings.OcrEnabled;
        _checkUpdates = settings.CheckUpdates;
        _scheduleEnabled = settings.ScheduleEnabled;
        _resetHourUtc = settings.ResetHourUtc;
        _resetMinuteUtc = settings.ResetMinuteUtc;
        _delayMinutes = settings.DelayMinutes;
        _scheduleMode = settings.ScheduleMode == "clock" ? "clock" : "reset";
        _scheduleHour = settings.ScheduleHour;
        _scheduleMinute = settings.ScheduleMinute;
        _dataRoot = SettingsStore.ResolveDataRoot(settings.DataRoot);
        _uiLanguage = App.UiLanguages.Contains(settings.UiLanguage) ? settings.UiLanguage : "";
        if (settings.Profiles.Count == 0) settings.Profiles.Add(new TaskProfile { Name = Strings.Get("Profile.Default") });
        foreach (var p in settings.Profiles) AddProfileItem(p);
        _selectedProfile = settings.Profiles[Math.Clamp(settings.ActiveProfile, 0, settings.Profiles.Count - 1)];

        _scheduleTimer = new DispatcherTimer(TimeSpan.FromSeconds(30), DispatcherPriority.Background,
                                             (_, _) => CheckSchedule(), dispatcher);
        PropertyChanged += OnSettingChanged;
    }

    public ObservableCollection<TaskItemViewModel> Tasks { get; } = new();
    public ObservableCollection<TaskProfile> Profiles { get; } = new();
    [ObservableProperty] private TaskProfile _selectedProfile;
    [ObservableProperty] private TaskItemViewModel? _selectedTask;
    public bool HasSelectedTask => SelectedTask is not null;

    partial void OnSelectedTaskChanged(TaskItemViewModel? value) => OnPropertyChanged(nameof(HasSelectedTask));
    [ObservableProperty] private string _summaryAccount = "";
    [ObservableProperty] private string _summaryLastRun = "";
    private IReadOnlyList<CoreTask> _coreTasks = Array.Empty<CoreTask>();
    private bool _loadingTasks;
    public ObservableCollection<LogEntry> Log { get; } = new();
    public ObservableCollection<string> AdbDevices { get; } = new();

    public IReadOnlyList<Choice> Controllers { get; } = new[]
    {
        new Choice("steam", Strings.Get("Settings.Controller.Steam")),
        new Choice("adb", Strings.Get("Settings.Controller.Adb")),
    };
    public IReadOnlyList<Choice> CaptureMethods { get; } = new[]
    {
        new Choice("bitblt", "BitBlt"),
        new Choice("printwindow", "PrintWindow"),
    };
    public IReadOnlyList<Choice> ScreencapMethods { get; } = new[]
    {
        new Choice("auto", Strings.Get("Settings.Auto")),
        new Choice("raw", "Raw"),
        new Choice("encode", "PNG"),
    };
    public IReadOnlyList<Choice> TouchMethods { get; } = new[]
    {
        new Choice("adb", "adb input"),
        new Choice("maatouch", "MaaTouch"),
    };
    // Language names are written in their own language, so they read the same in every UI language.
    public IReadOnlyList<Choice> UiLanguageChoices { get; } = new[]
    {
        new Choice("", Strings.Get("Settings.UiLanguage.Auto")),
        new Choice("ko", "한국어"),
        new Choice("en", "English"),
    };
    public IReadOnlyList<Choice> ScheduleModes { get; } = new[]
    {
        new Choice("reset", Strings.Get("Schedule.Mode.Reset")),
        new Choice("clock", Strings.Get("Schedule.Mode.Clock")),
    };

    [ObservableProperty] private string _controller;
    [ObservableProperty] private string _capture;
    [ObservableProperty] private bool _fitWindow;
    [ObservableProperty] private string _adbPath;
    [ObservableProperty] private string _adbSerial;
    [ObservableProperty] private string _adbScreencap;
    [ObservableProperty] private string _adbTouch;
    [ObservableProperty] private string _maaTouchPath;
    [ObservableProperty] private int _backKeycode;
    [ObservableProperty] private bool _startEmulator;
    [ObservableProperty] private string _emulatorPath;
    [ObservableProperty] private string _emulatorArgs;
    [ObservableProperty] private bool _startGame;
    [ObservableProperty] private bool _closeGameAfter;
    [ObservableProperty] private bool _closeEmulatorAfter;
    [ObservableProperty] private bool _steamStartGame;
    [ObservableProperty] private bool _steamCloseGame;
    [ObservableProperty] private bool _ocrEnabled;
    [ObservableProperty] private bool _checkUpdates;
    [ObservableProperty] private string _updateText = "";
    private string _updateUrl = "";
    [ObservableProperty] private bool _scheduleEnabled;
    [ObservableProperty] private int _resetHourUtc;
    [ObservableProperty] private int _resetMinuteUtc;
    [ObservableProperty] private int _delayMinutes;
    [ObservableProperty] private string _scheduleMode;
    [ObservableProperty] private int _scheduleHour;
    [ObservableProperty] private int _scheduleMinute;
    [ObservableProperty] private string _scheduleWarning = "";
    [ObservableProperty] private string _dataRoot;
    [ObservableProperty] private string _uiLanguage;

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(StartCommand), nameof(StopCommand), nameof(RefreshDevicesCommand))]
    private bool _isRunning;

    [ObservableProperty] private string _statusText = "";
    [ObservableProperty] private string _nextRunText = "";
    [ObservableProperty] private string _coreVersion = "";
    [ObservableProperty] private bool _coreAvailable;

    public bool IsAdb => Controller == "adb";
    public bool IsSteam => Controller == "steam";
    public bool IsClockSchedule => ScheduleMode == "clock";
    public bool IsResetSchedule => !IsClockSchedule;
    public string ResetLocalText =>
        Strings.Format("Settings.ResetLocal",
                       new DateTimeOffset(2000, 1, 1, ResetHourUtc, ResetMinuteUtc, 0, TimeSpan.Zero)
                           .ToLocalTime().ToString("HH:mm"));

    public void Initialize()
    {
        CreateCore();
        LoadAccounts();
        _initializing = false;
        UpdateNextRun();
        _scheduleTimer.Start();
        if (CheckUpdates) _ = CheckForUpdateAsync();
    }

    private async Task CheckForUpdateAsync()
    {
        if (await UpdateChecker.CheckAsync() is not { } update) return;
        _updateUrl = update.Url;
        UpdateText = Strings.Format("Update.Available", update.Version);
    }

    [RelayCommand]
    private void OpenUpdate()
    {
        if (_updateUrl.Length == 0) return;
        System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(_updateUrl) { UseShellExecute = true });
    }

    private void CreateCore()
    {
        _core?.Dispose();
        _core = null;
        _connected = false;
        Tasks.Clear();
        try
        {
            _core = new CoreClient(DataRoot, _settings.Language, OcrEnabled);
        }
        catch (Exception e) when (e is DllNotFoundException or EntryPointNotFoundException or InvalidOperationException
                                      or BadImageFormatException)
        {
            CoreAvailable = false;
            StatusText = Strings.Get("Status.CoreMissing");
            AddLog("error", e.Message);
            return;
        }
        CoreAvailable = true;
        CoreVersion = _core.Version;
        _core.EventReceived += e => _dispatcher.BeginInvoke(() => OnCoreEvent(e));
        var error = _core.LastError;
        if (!string.IsNullOrEmpty(error))
        {
            StatusText = Strings.Get("Status.DataError");
            AddLog("error", error);
        }
        _coreTasks = _core.GetTasks();
        LoadTasks();
        if (string.IsNullOrEmpty(error)) StatusText = Strings.Get("Status.Ready");
    }

    // The task list of the active profile, in its order.
    private void LoadTasks()
    {
        _loadingTasks = true;
        Tasks.Clear();
        var profile = SelectedProfile;
        var byId = _coreTasks.ToDictionary(t => t.Id);
        var ordered = profile.Order.Where(byId.ContainsKey).Select(id => byId[id])
                             .Concat(_coreTasks.Where(t => !profile.Order.Contains(t.Id)));
        foreach (var t in ordered)
            Tasks.Add(new TaskItemViewModel(t, profile.SelectedTasks.Contains(t.Id),
                                            profile.TaskSettings.GetValueOrDefault(t.Id), OnTaskSettingsChanged));
        foreach (var t in Tasks)
        {
            t.PropertyChanged += OnTaskSelectionChanged;
            t.ShowLastResult(_settings.LastResults.GetValueOrDefault(t.Id));
        }
        SelectedTask = Tasks.FirstOrDefault(t => t.Id == SelectedTask?.Id);
        _loadingTasks = false;
        StartCommand.NotifyCanExecuteChanged();
    }

    private void AddProfileItem(TaskProfile profile)
    {
        Profiles.Add(profile);
        profile.PropertyChanged += (_, _) => { if (!_initializing) Save(); };
    }

    partial void OnSelectedProfileChanged(TaskProfile value)
    {
        if (_initializing) return;
        _settings.ActiveProfile = Profiles.IndexOf(value);
        Save();
        LoadTasks();
    }

    [RelayCommand]
    private void AddProfile()
    {
        if (IsRunning) return;
        var copy = SelectedProfile.Copy(Strings.Format("Profile.NewName", Profiles.Count + 1));
        _settings.Profiles.Add(copy);
        AddProfileItem(copy);
        SelectedProfile = copy;
    }

    [RelayCommand]
    private void DeleteProfile()
    {
        if (IsRunning || Profiles.Count <= 1) return;
        var profile = SelectedProfile;
        if (!Views.ConfirmDialog.Ask(Strings.Get("Profile.Delete"), Strings.Format("Profile.DeleteQuestion", profile.Name),
                                     Strings.Get("Account.DeleteConfirm"), Strings.Get("Account.DeleteCancel"))) return;
        _settings.Profiles.Remove(profile);
        Profiles.Remove(profile);
        SelectedProfile = Profiles[0];
    }

    [RelayCommand]
    private void ResetOrder()
    {
        if (IsRunning) return;
        SelectedProfile.Order.Clear();
        Save();
        LoadTasks();
    }

    // Drag and drop in the task list: the task takes the place of the one it was dropped on.
    public void MoveTaskTo(TaskItemViewModel task, int to)
    {
        var from = Tasks.IndexOf(task);
        if (IsRunning || from < 0 || to < 0 || to >= Tasks.Count || from == to) return;
        Tasks.Move(from, to);
        SelectedProfile.Order = Tasks.Select(t => t.Id).ToList();
        Save();
    }

    private void OnTaskSettingsChanged()
    {
        if (_initializing || _loadingTasks) return;
        SelectedProfile.TaskSettings = Tasks.Where(t => t.HasSettings).ToDictionary(
            t => t.Id, t => t.SettingsJson().ToDictionary(kv => kv.Key, kv => kv.Value?.DeepClone()));
        Save();
    }

    private void OnTaskSelectionChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (e.PropertyName != nameof(TaskItemViewModel.IsSelected) || _loadingTasks) return;
        SelectedProfile.SelectedTasks = Tasks.Where(t => t.IsSelected).Select(t => t.Id).ToList();
        Save();
        StartCommand.NotifyCanExecuteChanged();
    }

    private void OnSettingChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (_initializing) return;
        switch (e.PropertyName)
        {
            case nameof(Controller):
                OnPropertyChanged(nameof(IsAdb));
                OnPropertyChanged(nameof(IsSteam));
                break;
            case nameof(ResetHourUtc):
            case nameof(ResetMinuteUtc):
                OnPropertyChanged(nameof(ResetLocalText));
                break;
            case nameof(ScheduleMode):
                OnPropertyChanged(nameof(IsClockSchedule));
                OnPropertyChanged(nameof(IsResetSchedule));
                break;
            case nameof(IsRunning):
            case nameof(StatusText):
            case nameof(NextRunText):
            case nameof(CoreVersion):
            case nameof(CoreAvailable):
            case nameof(IsAdb):
            case nameof(IsSteam):
            case nameof(ResetLocalText):
            case nameof(IsClockSchedule):
            case nameof(IsResetSchedule):
            case nameof(ScheduleWarning):
                return;
        }
        _settings.Controller = Controller;
        _settings.Capture = Capture;
        _settings.FitWindow = FitWindow;
        _settings.AdbPath = AdbPath;
        _settings.AdbSerial = AdbSerial;
        _settings.AdbScreencap = AdbScreencap;
        _settings.AdbTouch = AdbTouch;
        _settings.MaaTouchPath = MaaTouchPath;
        _settings.BackKeycode = BackKeycode;
        _settings.StartEmulator = StartEmulator;
        _settings.EmulatorPath = EmulatorPath;
        _settings.EmulatorArgs = EmulatorArgs;
        _settings.StartGame = StartGame;
        _settings.CloseGameAfter = CloseGameAfter;
        _settings.CloseEmulatorAfter = CloseEmulatorAfter;
        _settings.SteamStartGame = SteamStartGame;
        _settings.SteamCloseGame = SteamCloseGame;
        _settings.OcrEnabled = OcrEnabled;
        _settings.CheckUpdates = CheckUpdates;
        _settings.ScheduleEnabled = ScheduleEnabled;
        _settings.ResetHourUtc = Math.Clamp(ResetHourUtc, 0, 23);
        _settings.ResetMinuteUtc = Math.Clamp(ResetMinuteUtc, 0, 59);
        _settings.DelayMinutes = Math.Clamp(DelayMinutes, 0, 600);
        _settings.ScheduleMode = ScheduleMode;
        _settings.ScheduleHour = Math.Clamp(ScheduleHour, 0, 23);
        _settings.ScheduleMinute = Math.Clamp(ScheduleMinute, 0, 59);
        _settings.DataRoot = DataRoot;
        _settings.UiLanguage = UiLanguage;
        Save();
        _connected = false;  // connection settings may have changed: reconnect on the next start
        if (e.PropertyName is nameof(OcrEnabled) or nameof(DataRoot) && !IsRunning) CreateCore();
        UpdateNextRun();
    }

    private void Save()
    {
        try
        {
            _store.Save(_settings);
        }
        catch (Exception e) when (e is System.IO.IOException or UnauthorizedAccessException)
        {
            AddLog("warn", Strings.Get("Log.SaveFailed") + " " + e.Message);
        }
    }

    private JsonObject ConnectArgs() => Controller == "adb"
        ? new JsonObject
        {
            ["type"] = "adb",
            ["adb_path"] = AdbPath,
            ["serial"] = AdbSerial,
            ["screencap"] = AdbScreencap,
            ["touch"] = AdbTouch,
            ["maatouch_path"] = MaaTouchPath,
            ["back_keycode"] = BackKeycode,
        }
        : new JsonObject { ["type"] = "steam", ["capture"] = Capture, ["fit_window"] = FitWindow };

    // Emulator runs with start/stop options: Core starts the emulator and the game and connects on
    // its worker (this can take minutes), so the UI thread does not connect itself.
    private JsonObject? PrepareArgs()
    {
        if (Controller == "steam")
        {
            if (!(SteamStartGame || SteamCloseGame)) return null;
            return new JsonObject
            {
                ["connect"] = ConnectArgs(),
                ["launch"] = new JsonObject { ["start_game"] = SteamStartGame },
                ["finish"] = new JsonObject { ["close_game"] = SteamCloseGame },
            };
        }
        if (Controller != "adb" || !(StartEmulator || StartGame || CloseGameAfter || CloseEmulatorAfter)) return null;
        return new JsonObject
        {
            ["connect"] = ConnectArgs(),
            ["launch"] = new JsonObject
            {
                ["emulator_path"] = StartEmulator ? EmulatorPath.Trim().Trim('"') : "",
                ["emulator_args"] = StartEmulator ? EmulatorArgs : "",
                ["start_game"] = StartGame,
            },
            ["finish"] = new JsonObject
            {
                ["close_game"] = CloseGameAfter,
                ["close_emulator"] = CloseEmulatorAfter,
            },
        };
    }

    private bool EnsureConnected()
    {
        if (_core is null) return false;
        var args = ConnectArgs();
        var key = args.ToJsonString();
        if (_connected && key == _connectedWith) return true;
        StatusText = Strings.Get("Status.Connecting");
        if (!_core.Connect(args))
        {
            StatusText = Strings.Get("Status.ConnectFailed");
            AddLog("error", _core.LastError);
            _connected = false;
            return false;
        }
        _connected = true;
        _connectedWith = key;
        return true;
    }

    private bool CanStart() => CoreAvailable && !IsRunning && Tasks.Any(t => t.IsSelected);

    [RelayCommand(CanExecute = nameof(CanStart))]
    private void Start() => StartRun(scheduled: false);

    private void StartRun(bool scheduled)
    {
        if (_core is null || IsRunning) return;
        var selected = Tasks.Where(t => t.IsSelected).Select(t => t.Id).ToList();
        if (selected.Count == 0) return;
        var prepare = PrepareArgs();
        if (prepare is null && !EnsureConnected()) return;
        if (prepare is not null) _connected = false;  // Core replaces its controller on the worker
        foreach (var t in Tasks) t.SetStatus(t.IsSelected ? "pending" : "", "");
        var settings = new JsonObject();
        foreach (var t in Tasks.Where(t => t.IsSelected && t.HasSettings)) settings[t.Id] = t.SettingsJson();
        if (!_core.Start(selected, settings, prepare))
        {
            StatusText = Strings.Get("Status.StartFailed");
            AddLog("error", _core.LastError);
            return;
        }
        _runIsScheduled = scheduled;
        IsRunning = true;
        StatusText = Strings.Get(scheduled ? "Status.RunningScheduled" : "Status.Running");
    }

    [RelayCommand(CanExecute = nameof(IsRunning))]
    private void Stop()
    {
        if (_core?.Stop() == true) StatusText = Strings.Get("Status.Stopping");
    }

    private bool CanRefreshDevices() => CoreAvailable && !IsRunning;

    [RelayCommand(CanExecute = nameof(CanRefreshDevices))]
    private void RefreshDevices()
    {
        if (_core is null) return;
        AdbDevices.Clear();
        foreach (var d in _core.ListAdbDevices(AdbPath))
            AdbDevices.Add(d.State == "device" ? d.Serial : $"{d.Serial} ({d.State})");
        if (AdbDevices.Count == 0)
            AddLog("warn", string.IsNullOrEmpty(_core.LastError) ? Strings.Get("Log.NoDevices") : _core.LastError);
    }

    [RelayCommand]
    private void ReloadData()
    {
        if (IsRunning) return;
        CreateCore();
    }

    [RelayCommand]
    private void ClearLog() => Log.Clear();

    private static string AccountsFolder =>
        System.IO.Path.Combine(System.IO.Path.GetDirectoryName(SettingsStore.FilePath)!, "accounts");

    // One box per accounts/<uid>.json: a new UID seen by the account record gets its own file.
    private void LoadAccounts()
    {
        Accounts.Clear();
        foreach (var card in AccountCard.LoadAll(AccountsFolder)) Accounts.Add(card);
        OnPropertyChanged(nameof(HasAccounts));
        UpdateSummary();
    }

    // The right side of the tasks tab when no task is chosen.
    private void UpdateSummary()
    {
        var card = Accounts.FirstOrDefault();
        SummaryAccount = card is null
            ? Strings.Get("Summary.NoAccount")
            : Strings.Format("Summary.AccountFormat", card.Ap, card.Pyroxene, card.Updated);
        var run = _settings.LastRun;
        SummaryLastRun = run is null
            ? Strings.Get("Summary.NoRun")
            : Strings.Format(run.Cancelled ? "Summary.RunCancelledFormat" : "Summary.RunFormat",
                             run.Time.ToLocalTime().ToString("MM-dd HH:mm"), run.Succeeded, run.Failed, run.Skipped);
    }

    [RelayCommand]
    private void ClearSelectedTask() => SelectedTask = null;

    // The history lives in the same file, so this cannot be undone; the next account record starts over.
    [RelayCommand]
    private void DeleteAccount(AccountCard card)
    {
        var question = string.Format(Strings.Get("Account.DeleteQuestion"), card.Nickname, card.Uid);
        if (!Views.ConfirmDialog.Ask(Strings.Get("Account.Delete"), question,
                                     Strings.Get("Account.DeleteConfirm"), Strings.Get("Account.DeleteCancel"))) return;
        try
        {
            System.IO.File.Delete(System.IO.Path.Combine(AccountsFolder, card.Uid + ".json"));
        }
        catch (Exception e) when (e is System.IO.IOException or UnauthorizedAccessException)
        {
            StatusText = e.Message;
        }
        LoadAccounts();
    }

    private void OnCoreEvent(JsonObject e)
    {
        switch (e["type"]?.GetValue<string>())
        {
            case "log":
                AddLog(e["level"]?.GetValue<string>() ?? "info", e["message"]?.GetValue<string>() ?? "");
                break;
            case "task":
            {
                var id = e["id"]?.GetValue<string>();
                var task = Tasks.FirstOrDefault(t => t.Id == id);
                var status = e["status"]?.GetValue<string>() ?? "";
                var message = e["message"]?.GetValue<string>() ?? "";
                task?.SetStatus(status, message);
                if (id is not null && status is "succeeded" or "failed" or "skipped" or "cancelled")
                {
                    var result = new TaskResult(DateTimeOffset.UtcNow, status, message);
                    _settings.LastResults[id] = result;
                    task?.ShowLastResult(result);
                }
                break;
            }
            case "failure_frame":
                AddLog("warn", Strings.Format("Log.FailureFrame", e["path"]?.GetValue<string>() ?? ""));
                break;
            case "run" when e["status"]?.GetValue<string>() == "finished":
            {
                IsRunning = false;
                LoadAccounts();
                var ok = e["ok"]?.GetValue<bool>() ?? false;
                var cancelled = e["cancelled"]?.GetValue<bool>() ?? false;
                var error = e["error"]?.GetValue<string>() ?? "";
                if (!string.IsNullOrEmpty(error)) AddLog("error", error);
                StatusText = Strings.Get(cancelled ? "Status.Cancelled" : ok ? "Status.Finished" : "Status.FinishedWithErrors");
                _settings.LastRun = new RunSummary(DateTimeOffset.UtcNow,
                    Tasks.Count(t => t.Status == "succeeded"), Tasks.Count(t => t.Status == "failed"),
                    Tasks.Count(t => t.Status == "skipped"), cancelled);
                Save();
                UpdateSummary();
                if (_runIsScheduled)
                {
                    // A scheduled day counts as done even after failures: retrying in a loop
                    // would hammer the game. The log says what failed.
                    _settings.LastScheduledRun = DateTimeOffset.UtcNow;
                    Save();
                }
                UpdateNextRun();
                break;
            }
        }
    }

    private void AddLog(string level, string message)
    {
        if (string.IsNullOrEmpty(message)) return;
        Log.Add(new LogEntry(DateTime.Now, level, message));
        while (Log.Count > MaxLogEntries) Log.RemoveAt(0);
    }

    private ScheduleInfo? QuerySchedule() =>
        CoreClient.NextRun(DateTimeOffset.UtcNow, _settings.LastScheduledRun, ResetHourUtc, ResetMinuteUtc,
                           DelayMinutes, ScheduleMode, ScheduleHour, ScheduleMinute);

    // A run started shortly before the server reset may still be going when the game day changes.
    private const int NearResetMinutes = 60;

    private void UpdateNextRun()
    {
        if (!ScheduleEnabled || !CoreAvailable)
        {
            NextRunText = Strings.Get("Schedule.Off");
            ScheduleWarning = "";
            return;
        }
        var info = QuerySchedule();
        NextRunText = info is null
            ? Strings.Get("Schedule.Invalid")
            : Strings.Format("Schedule.Next", info.Next.ToLocalTime().ToString("yyyy-MM-dd HH:mm"));
        ScheduleWarning = info?.MinutesBeforeReset is int m && m < NearResetMinutes
            ? Strings.Format("Schedule.NearReset", m)
            : "";
    }

    private void CheckSchedule()
    {
        UpdateNextRun();
        if (!ScheduleEnabled || !CoreAvailable || IsRunning) return;
        var info = QuerySchedule();
        if (info?.Due != true) return;
        AddLog("info", Strings.Get("Log.ScheduledStart"));
        StartRun(scheduled: true);
        if (!IsRunning)
        {
            // Could not start (not connected, nothing selected): do not retry every 30 s today.
            _settings.LastScheduledRun = DateTimeOffset.UtcNow;
            Save();
            UpdateNextRun();
        }
    }

    public void Dispose()
    {
        _scheduleTimer.Stop();
        _core?.Dispose();
        _core = null;
    }
}
