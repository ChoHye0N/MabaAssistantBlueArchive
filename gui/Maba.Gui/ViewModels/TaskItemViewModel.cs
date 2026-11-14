// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Collections.ObjectModel;
using System.Text.Json.Nodes;
using CommunityToolkit.Mvvm.ComponentModel;
using Maba.Gui.Native;

namespace Maba.Gui.ViewModels;

public partial class TaskItemViewModel : ObservableObject
{
    public TaskItemViewModel(CoreTask task, bool selected, IReadOnlyDictionary<string, JsonNode?>? saved,
                             Action settingsChanged)
    {
        foreach (var s in task.Settings)
            Settings.Add(new SettingItemViewModel(task.Id, s, saved is not null && saved.TryGetValue(s.Key, out var v) ? v : null,
                                                  settingsChanged));
        Id = task.Id;
        var localized = Strings.Get("Task." + task.Id);
        Name = localized == "Task." + task.Id ? task.Name : localized;
        Implemented = task.Implemented;
        NeedsOcr = task.NeedsOcr;
        // Under the title, in the display language: the tasks this one waits for.
        Meta = task.After.Count == 0
            ? ""
            : Strings.Get("Tasks.After") + ": " + string.Join(", ", task.After.Select(DisplayName));
        _isSelected = selected;
        _statusText = Implemented ? "" : Strings.Get("Task.NotImplemented");
        Description = Optional("TaskInfo." + task.Id);
        Warning = Optional("TaskWarn." + task.Id);
    }

    // A task's name in the display language; its id when the table has none.
    private static string DisplayName(string id) => Strings.Get("Task." + id) is var s && s != "Task." + id ? s : id;

    private static string Optional(string key) => Strings.Get(key) is var s && s != key ? s : "";

    public string Id { get; }
    public string Name { get; }
    public bool Implemented { get; }
    public bool NeedsOcr { get; }
    public string Meta { get; }
    public ObservableCollection<SettingItemViewModel> Settings { get; } = new();
    public bool HasSettings => Settings.Count > 0;
    public string Description { get; }
    public string Warning { get; }

    [ObservableProperty]
    private string _shortStatus = "";

    [ObservableProperty]
    private string _lastResult = "";

    public void ShowLastResult(Services.TaskResult? r)
    {
        LastResult = r is null ? "" : $"{r.Time.ToLocalTime():MM-dd HH:mm} · {Strings.Get("Status." + r.Status)}"
                                       + (string.IsNullOrEmpty(r.Message) ? "" : $": {r.Message}");
    }

    public JsonObject SettingsJson()
    {
        var o = new JsonObject();
        foreach (var s in Settings) o[s.Key] = s.ToJson();
        return o;
    }

    [ObservableProperty]
    private bool _isSelected;

    [ObservableProperty]
    private string _status = "";

    [ObservableProperty]
    private string _statusText;

    public void SetStatus(string status, string message)
    {
        Status = status;
        if (string.IsNullOrEmpty(status))
        {
            StatusText = Implemented ? "" : Strings.Get("Task.NotImplemented");
            ShortStatus = "";
            return;
        }
        var label = Strings.Get("Status." + status);
        ShortStatus = label;
        StatusText = string.IsNullOrEmpty(message) ? label : $"{label}: {message}";
    }
}
