// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.Collections.Specialized;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
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
            if (DataContext is MainViewModel vm) vm.Log.CollectionChanged += ScrollLogToEnd;
        };
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
