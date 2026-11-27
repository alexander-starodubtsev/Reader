#include "goodreader/domain/types.hpp"

#include <date/date.h>
#include <date/tz.h>

#include <algorithm>
#include <map>
#include <stdexcept>

namespace goodreader::domain {
namespace {

[[nodiscard]] LocalDate local_date_at(Timestamp instant, const date::time_zone* zone) {
    const date::zoned_time<Milliseconds> zoned_time(zone, instant);
    const auto local_day = date::floor<date::days>(zoned_time.get_local_time());
    const date::year_month_day calendar_date{local_day};
    return LocalDate(static_cast<int>(calendar_date.year()),
                     static_cast<unsigned>(calendar_date.month()),
                     static_cast<unsigned>(calendar_date.day()));
}

[[nodiscard]] Timestamp next_local_day_start(Timestamp instant, const date::time_zone* zone) {
    const date::zoned_time<Milliseconds> zoned_time(zone, instant);
    const auto local_day = date::floor<date::days>(zoned_time.get_local_time());
    const auto next_local_midnight = date::local_time<Milliseconds>{
        std::chrono::duration_cast<Milliseconds>((local_day + date::days{1}).time_since_epoch())};
    return zone->to_sys(next_local_midnight, date::choose::earliest);
}

} // namespace

LocalDate::LocalDate(int year, unsigned month, unsigned day) : year(year), month(month), day(day) {
    const date::year_month_day value{date::year{year}, date::month{month}, date::day{day}};
    if (!value.ok()) {
        throw std::invalid_argument("Calendar date is invalid.");
    }
}

LocalDate LocalDate::plus_days(int days) const {
    const date::year_month_day value{date::year{year}, date::month{month}, date::day{day}};
    const date::year_month_day result{date::sys_days{value} + date::days{days}};
    return LocalDate(static_cast<int>(result.year()), static_cast<unsigned>(result.month()),
                     static_cast<unsigned>(result.day()));
}

std::set<LocalDate> StreakCalculator::qualifying_dates(const std::vector<ReadingSession>& sessions,
                                                       StreakPolicy policy) {
    std::map<LocalDate, std::int64_t> active_by_date;
    for (const auto& session : sessions) {
        const auto* zone = date::locate_zone(session.time_zone_id);
        for (const auto& interval : session.active_intervals) {
            auto cursor = interval.started_at;
            while (cursor < interval.ended_at) {
                const auto local_date = local_date_at(cursor, zone);
                const auto segment_end =
                    std::min(interval.ended_at, next_local_day_start(cursor, zone));
                const auto duration = (segment_end - cursor).count();
                active_by_date[local_date] += duration;
                cursor = segment_end;
            }
        }
    }

    std::set<LocalDate> qualifying;
    for (const auto& [local_date, active_millis] : active_by_date) {
        if (active_millis >= policy.minimum_active_millis_per_day) {
            qualifying.insert(local_date);
        }
    }
    return qualifying;
}

std::size_t StreakCalculator::current_streak(const std::set<LocalDate>& qualifying_dates,
                                             LocalDate today) {
    auto cursor = today;
    if (!qualifying_dates.contains(cursor)) {
        cursor = cursor.plus_days(-1);
        if (!qualifying_dates.contains(cursor)) {
            return 0;
        }
    }

    std::size_t count = 0;
    while (qualifying_dates.contains(cursor)) {
        ++count;
        cursor = cursor.plus_days(-1);
    }
    return count;
}

} // namespace goodreader::domain
