// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include "maba/schedule.h"

namespace maba {

namespace {

constexpr std::int64_t kDay = 24 * 60 * 60;

// Floor division that also works for negative numerators.
std::int64_t FloorDiv(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

}  // namespace

bool IsValid(const DailySchedule& s) {
    return s.reset_hour_utc >= 0 && s.reset_hour_utc < 24 && s.reset_minute_utc >= 0 &&
           s.reset_minute_utc < 60 && s.delay_minutes >= 0 && s.delay_minutes < 24 * 60;
}

std::int64_t LastReset(std::int64_t now, const DailySchedule& s) {
    const std::int64_t offset = s.reset_hour_utc * 3600LL + s.reset_minute_utc * 60LL;
    return FloorDiv(now - offset, kDay) * kDay + offset;
}

std::int64_t CurrentSlot(std::int64_t now, const DailySchedule& s) {
    return LastReset(now, s) + s.delay_minutes * 60LL;
}

bool IsDue(std::int64_t now, std::int64_t last_run, const DailySchedule& s) {
    const std::int64_t slot = CurrentSlot(now, s);
    return now >= slot && last_run < slot;
}

std::int64_t NextRun(std::int64_t now, std::int64_t last_run, const DailySchedule& s) {
    if (IsDue(now, last_run, s)) return now;
    const std::int64_t slot = CurrentSlot(now, s);
    return slot > now ? slot : slot + kDay;
}

bool ClockSchedule(int hour, int minute, int utc_offset_minutes, DailySchedule& out) {
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
    if (utc_offset_minutes < -14 * 60 || utc_offset_minutes > 14 * 60) return false;
    constexpr int kDayMinutes = 24 * 60;
    const int utc = ((hour * 60 + minute - utc_offset_minutes) % kDayMinutes + kDayMinutes) % kDayMinutes;
    out.reset_hour_utc = utc / 60;
    out.reset_minute_utc = utc % 60;
    out.delay_minutes = 0;
    return true;
}

int MinutesBeforeReset(const DailySchedule& clock, const DailySchedule& server) {
    constexpr int kDayMinutes = 24 * 60;
    const int slot = (clock.reset_hour_utc * 60 + clock.reset_minute_utc + clock.delay_minutes) % kDayMinutes;
    const int reset = server.reset_hour_utc * 60 + server.reset_minute_utc;
    return ((reset - slot) % kDayMinutes + kDayMinutes) % kDayMinutes;
}

}  // namespace maba
