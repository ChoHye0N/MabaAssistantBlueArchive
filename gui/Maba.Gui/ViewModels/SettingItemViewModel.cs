// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Collections.ObjectModel;
using System.Text.Json.Nodes;
using CommunityToolkit.Mvvm.ComponentModel;
using Maba.Gui.Native;

namespace Maba.Gui.ViewModels;

// One task setting as declared in tasks/<id>.json. Core validates the values again on start.
public partial class SettingItemViewModel : ObservableObject
{
    private readonly CoreSetting _setting;
    private readonly Action _changed;
    private bool _loading;

    public SettingItemViewModel(string taskId, CoreSetting setting, JsonNode? saved, Action changed)
    {
        _setting = setting;
        _changed = changed;
        Key = setting.Key;
        var localized = Strings.Get($"Setting.{taskId}.{setting.Key}");
        Label = localized == $"Setting.{taskId}.{setting.Key}" ? setting.Description : localized;
        Choices = setting.Choices;
        Min = setting.Min ?? 0;
        Max = setting.Max ?? 0;
        if (IsChecklist)
        {
            foreach (var c in setting.Choices)
                Items.Add(new ChecklistItem(c, setting.Labels.TryGetValue(c, out var l) ? l : c, Changed));
        }
        Load(saved ?? setting.Default);
    }

    public string Key { get; }
    public string Label { get; }
    public IReadOnlyList<string> Choices { get; }
    public int Min { get; }
    public int Max { get; }
    public bool IsBool => _setting.Type == "bool";
    public bool IsInt => _setting.Type == "int";
    public bool IsChoice => _setting.Type == "choice";
    public bool IsChecklist => _setting.Type == "list" && _setting.Ui == "checklist";
    public bool IsList => _setting.Type == "list" && !IsChecklist;
    public ObservableCollection<ChecklistItem> Items { get; } = new();
    // Allowed items, or an example when any text is allowed (student names).
    public string ListHint => Choices.Count == 0 ? Strings.Get("Setting.ListExample") : string.Join(", ", Choices);

    [ObservableProperty] private bool _boolValue;
    [ObservableProperty] private double _intValue;
    [ObservableProperty] private string _choiceValue = "";
    [ObservableProperty] private string _listText = "";

    partial void OnBoolValueChanged(bool value) => Changed();
    partial void OnIntValueChanged(double value) => Changed();
    partial void OnChoiceValueChanged(string value) => Changed();
    partial void OnListTextChanged(string value) => Changed();

    private void Changed()
    {
        if (!_loading) _changed();
    }

    private void Load(JsonNode? value)
    {
        _loading = true;
        try
        {
            switch (_setting.Type)
            {
                case "bool": BoolValue = value?.GetValue<bool>() ?? false; break;
                case "int": IntValue = value?.GetValue<long>() ?? Min; break;
                case "choice": ChoiceValue = value?.GetValue<string>() ?? ""; break;
                case "list":
                    var items = value?.AsArray().Select(v => v?.GetValue<string>() ?? "").ToList() ?? new List<string>();
                    ListText = string.Join(", ", items);
                    foreach (var i in Items) i.IsChecked = items.Contains(i.Key);
                    break;
            }
        }
        catch (Exception e) when (e is InvalidOperationException or FormatException)
        {
            if (!ReferenceEquals(value, _setting.Default)) Load(_setting.Default);  // a stale saved value
        }
        finally
        {
            _loading = false;
        }
    }

    public JsonNode? ToJson() => _setting.Type switch
    {
        "bool" => JsonValue.Create(BoolValue),
        "int" => JsonValue.Create((long)Math.Round(IntValue)),
        "choice" => JsonValue.Create(ChoiceValue),
        _ when IsChecklist => new JsonArray(Items.Where(i => i.IsChecked).Select(i => (JsonNode?)JsonValue.Create(i.Key)).ToArray()),
        _ => new JsonArray(ListText.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
                                   .Select(s => (JsonNode?)JsonValue.Create(s)).ToArray()),
    };
}

// One check box of a checklist setting (choices shown with their labels).
public partial class ChecklistItem : ObservableObject
{
    private readonly Action _changed;

    public ChecklistItem(string key, string label, Action changed)
    {
        Key = key;
        Label = label;
        _changed = changed;
    }

    public string Key { get; }
    public string Label { get; }

    [ObservableProperty] private bool _isChecked;

    partial void OnIsCheckedChanged(bool value) => _changed();
}
