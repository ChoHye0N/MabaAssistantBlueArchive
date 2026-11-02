// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Daily schedule keyed to the server reset time (not local midnight). Pure arithmetic on
// Unix seconds (UTC), so it is testable and independent of the machine's time zone.
#pragma once

#include <cstdint>
#include <string>

namespace maba {

struct DailySchedule {
    // Server daily reset in UTC. Default 19:00 UTC (04:00 KST): verify in the game.
    int reset_hour_utc = 19;
    int reset_minute_utc = 0;
    int delay_minutes = 5;  // run this long after the reset, when servers have settled
};

bool IsValid(const DailySchedule& s);

// The most recent reset at or before `now`.
std::int64_t LastReset(std::int64_t now, const DailySchedule& s);

// The run slot of the current game day: LastReset(now) + delay.
std::int64_t CurrentSlot(std::int64_t now, const DailySchedule& s);

// True when today's slot has passed and the last run (0 = never) happened before it.
bool IsDue(std::int64_t now, std::int64_t last_run, const DailySchedule& s);

// When the next scheduled run should start: `now` if due, else the next slot after `now`
// (today's slot if it is still ahead, otherwise tomorrow's).
std::int64_t NextRun(std::int64_t now, std::int64_t last_run, const DailySchedule& s);

}  // namespace maba
