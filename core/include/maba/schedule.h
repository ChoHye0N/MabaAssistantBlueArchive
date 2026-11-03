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

// A fixed local clock time each day (e.g. 07:30). `utc_offset_minutes` is the local offset
// from UTC at the time of the check (KST = +540); the caller passes the current one, so a
// daylight-saving change moves the slot with the clock. The result is the same daily slot
// expressed as a UTC anchor with no delay. False when a value is out of range.
bool ClockSchedule(int hour, int minute, int utc_offset_minutes, DailySchedule& out);

// Minutes from the slot of `clock` to the next server reset of `server` (0..1439).
// A small value means a run would still be going when the game day changes.
int MinutesBeforeReset(const DailySchedule& clock, const DailySchedule& server);

}  // namespace maba
