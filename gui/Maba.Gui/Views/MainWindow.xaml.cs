// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Collections.Specialized;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Threading;
using Maba.Gui.ViewModels;

namespace Maba.Gui.Views;

public partial class MainWindow : HandyControl.Controls.Window
{
    public MainWindow()
    {
        InitializeComponent();
        DataContextChanged += (_, _) =>
        {
            if (DataContext is not MainViewModel vm) return;
            vm.Log.CollectionChanged += ScrollLogToEnd;
            vm.PropertyChanged += (_, e) =>
            {
                if (e.PropertyName is nameof(MainViewModel.IsRunning) or nameof(MainViewModel.StopHotkey)) UpdateStopHotkey();
            };
        };
        SourceInitialized += (_, _) =>
        {
            _source = HwndSource.FromHwnd(new WindowInteropHelper(this).Handle);
            _source?.AddHook(OnWindowMessage);
            UpdateStopHotkey();
        };
        Closed += (_, _) => ReleaseStopHotkey();
    }

    // Stop key: Ctrl+Alt+S from any window while a run is going. A run moves the real mouse and
    // keeps the game in front, so the stop button is hard to reach. The key is held only during a
    // run, so the combination stays free for other programs the rest of the time.
    private const int StopHotkeyId = 0x4D42;
    private const int WmHotkey = 0x0312;
    private const uint ModAlt = 0x0001, ModControl = 0x0002, ModNoRepeat = 0x4000;
    private const uint KeyS = 0x53;
    private HwndSource? _source;
    private bool _stopHotkeyHeld;

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    private void UpdateStopHotkey()
    {
        if (_source is null || DataContext is not MainViewModel vm) return;
        var wanted = vm.IsRunning && vm.StopHotkey;
        if (wanted == _stopHotkeyHeld) return;
        if (!wanted)
        {
            ReleaseStopHotkey();
            return;
        }
        _stopHotkeyHeld = RegisterHotKey(_source.Handle, StopHotkeyId, ModControl | ModAlt | ModNoRepeat, KeyS);
        if (!_stopHotkeyHeld) vm.NoteStopHotkeyUnavailable();
    }

    private void ReleaseStopHotkey()
    {
        if (_stopHotkeyHeld && _source is not null) UnregisterHotKey(_source.Handle, StopHotkeyId);
        _stopHotkeyHeld = false;
    }

    private IntPtr OnWindowMessage(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        if (msg == WmHotkey && wParam.ToInt32() == StopHotkeyId)
        {
            (DataContext as MainViewModel)?.StopByHotkey();
            handled = true;
        }
        return IntPtr.Zero;
    }

    // Task order: a task is dragged by its grip and dropped on another task, taking its place.
    private Point _dragStart;
    private TaskItemViewModel? _dragItem;

    private void OnGripDown(object sender, MouseButtonEventArgs e)
    {
        _dragStart = e.GetPosition(this);
        _dragItem = (sender as FrameworkElement)?.DataContext as TaskItemViewModel;
    }

    private void OnGripMove(object sender, MouseEventArgs e)
    {
        if (_dragItem is null || e.LeftButton != MouseButtonState.Pressed) return;
        var now = e.GetPosition(this);
        if (Math.Abs(now.X - _dragStart.X) < SystemParameters.MinimumHorizontalDragDistance &&
            Math.Abs(now.Y - _dragStart.Y) < SystemParameters.MinimumVerticalDragDistance) return;
        var item = _dragItem;
        _dragItem = null;
        DragDrop.DoDragDrop((DependencyObject)sender, item, DragDropEffects.Move);
    }

    private void OnTaskDragOver(object sender, DragEventArgs e)
    {
        var ok = e.Data.GetDataPresent(typeof(TaskItemViewModel));
        e.Effects = ok ? DragDropEffects.Move : DragDropEffects.None;
        if (ok && sender is Border border) border.BorderBrush = (Brush)FindResource("PrimaryBrush");
        e.Handled = true;
    }

    private void OnTaskDragLeave(object sender, DragEventArgs e)
    {
        if (sender is Border border) border.BorderBrush = (Brush)FindResource("BorderBrush");
    }

    private void OnTaskDrop(object sender, DragEventArgs e)
    {
        OnTaskDragLeave(sender, e);
        if (e.Data.GetData(typeof(TaskItemViewModel)) is not TaskItemViewModel dragged) return;
        if ((sender as FrameworkElement)?.DataContext is not TaskItemViewModel target) return;
        if (DataContext is MainViewModel vm) vm.MoveTaskTo(dragged, vm.Tasks.IndexOf(target));
        e.Handled = true;
    }

    // A long list scrolls while a task is dragged near its top or bottom edge.
    private void OnTaskListDragOver(object sender, DragEventArgs e)
    {
        if (FindChild<ScrollViewer>(sender as DependencyObject) is not { } scroll) return;
        var y = e.GetPosition(scroll).Y;
        if (y < 40) scroll.ScrollToVerticalOffset(scroll.VerticalOffset - 12);
        else if (y > scroll.ActualHeight - 40) scroll.ScrollToVerticalOffset(scroll.VerticalOffset + 12);
    }

    private static T? FindChild<T>(DependencyObject? parent) where T : DependencyObject
    {
        if (parent is null) return null;
        for (var i = 0; i < VisualTreeHelper.GetChildrenCount(parent); ++i)
        {
            var child = VisualTreeHelper.GetChild(parent, i);
            if (child is T found) return found;
            if (FindChild<T>(child) is { } deeper) return deeper;
        }
        return null;
    }

    // The newest line is at the bottom. Scrolling waits until the list has laid out the new line
    // (ScrollIntoView on a virtualized list jumped back to the top), and the tab opens at the end too.
    private void ScrollLogToEnd(object? sender, NotifyCollectionChangedEventArgs e) => ScrollLogToEnd();

    private void ScrollLogToEnd() =>
        Dispatcher.BeginInvoke(DispatcherPriority.Background, () => FindChild<ScrollViewer>(LogList)?.ScrollToEnd());

    private void OnLogVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
    {
        if (e.NewValue is true) ScrollLogToEnd();
    }
}
