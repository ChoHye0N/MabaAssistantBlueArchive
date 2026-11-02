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
        After = task.After.Count == 0 ? "" : string.Join(", ", task.After);
        _isSelected = selected;
        _statusText = Implemented ? "" : Strings.Get("Task.NotImplemented");
    }

    public string Id { get; }
    public string Name { get; }
    public bool Implemented { get; }
    public bool NeedsOcr { get; }
    public string After { get; }
    public ObservableCollection<SettingItemViewModel> Settings { get; } = new();
    public bool HasSettings => Settings.Count > 0;

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
            return;
        }
        var label = Strings.Get("Status." + status);
        StatusText = string.IsNullOrEmpty(message) ? label : $"{label}: {message}";
    }
}
