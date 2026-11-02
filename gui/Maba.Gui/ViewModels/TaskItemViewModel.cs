// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using CommunityToolkit.Mvvm.ComponentModel;
using Maba.Gui.Native;

namespace Maba.Gui.ViewModels;

public partial class TaskItemViewModel : ObservableObject
{
    public TaskItemViewModel(CoreTask task, bool selected)
    {
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
