// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
using System.ComponentModel;
using System.Diagnostics;
using System.IO;

namespace Maba.Gui.Services;

// Turning the PC off after a run, through the system's own shutdown command: Windows shows its
// notice and counts down, and the shutdown can be called off until the time is up.
public static class PowerControl
{
    public const int ShutdownDelaySeconds = 60;

    public static bool ScheduleShutdown() => Run($"/s /t {ShutdownDelaySeconds}");

    public static bool CancelShutdown() => Run("/a");

    private static bool Run(string arguments)
    {
        try
        {
            using var process = Process.Start(new ProcessStartInfo
            {
                FileName = Path.Combine(Environment.SystemDirectory, "shutdown.exe"),
                Arguments = arguments,
                UseShellExecute = false,
                CreateNoWindow = true,
            });
            if (process is null) return false;
            return process.WaitForExit(10000) && process.ExitCode == 0;
        }
        catch (Exception e) when (e is Win32Exception or IOException or InvalidOperationException)
        {
            return false;
        }
    }
}
