package com.goodreader.domain

import kotlinx.datetime.DatePeriod
import kotlinx.datetime.Instant
import kotlinx.datetime.LocalDate
import kotlinx.datetime.TimeZone
import kotlinx.datetime.atStartOfDayIn
import kotlinx.datetime.plus
import kotlinx.datetime.toLocalDateTime
import kotlin.time.Duration.Companion.minutes

/** A half-open interval of active reading time. */
data class ActiveReadingInterval(
    val startedAt: Instant,
    val endedAt: Instant,
) {
    init {
        require(endedAt > startedAt) { "An active interval must have positive duration." }
    }
}

/** A recorded reading session with active intervals and the time zone used during it. */
data class ReadingSession(
    val id: String,
    val bookId: BookId,
    val timeZoneId: String,
    val activeIntervals: List<ActiveReadingInterval>,
) {
    init {
        require(id.isNotBlank()) { "Reading session ID must not be blank." }
        require(timeZoneId.isNotBlank()) { "Time-zone ID must not be blank." }
        require(activeIntervals.zipWithNext().all { (left, right) -> left.endedAt <= right.startedAt }) {
            "Active reading intervals must be ordered and non-overlapping."
        }
    }

    /** Total active reading time in milliseconds. */
    val activeMillis: Long
        get() = activeIntervals.sumOf { (it.endedAt - it.startedAt).inWholeMilliseconds }
}

/** Rules for converting active reading intervals into qualifying calendar days. */
data class StreakPolicy(
    val minimumActiveMillisPerDay: Long = 10.minutes.inWholeMilliseconds,
) {
    init {
        require(minimumActiveMillisPerDay > 0) { "Daily reading threshold must be positive." }
    }
}

/** Calculates qualifying local dates and consecutive reading streaks. */
object StreakCalculator {
    /** Returns dates on which active reading met the daily threshold. */
    fun qualifyingDates(
        sessions: List<ReadingSession>,
        policy: StreakPolicy = StreakPolicy(),
    ): Set<LocalDate> {
        val activeByDate = mutableMapOf<LocalDate, Long>()
        sessions.forEach { session ->
            val zone = TimeZone.of(session.timeZoneId)
            session.activeIntervals.forEach { interval ->
                accumulateByLocalDate(interval, zone, activeByDate)
            }
        }
        return activeByDate.filterValues { it >= policy.minimumActiveMillisPerDay }.keys
    }

    /**
     * Returns the current streak ending today, or yesterday when today has not yet qualified.
     * A missed calendar day resets the count.
     */
    fun currentStreak(
        qualifyingDates: Set<LocalDate>,
        today: LocalDate,
    ): Int {
        var cursor =
            when {
                today in qualifyingDates -> today
                today.minusOneDay() in qualifyingDates -> today.minusOneDay()
                else -> return 0
            }
        var count = 0
        while (cursor in qualifyingDates) {
            count += 1
            cursor = cursor.minusOneDay()
        }
        return count
    }

    private fun accumulateByLocalDate(
        interval: ActiveReadingInterval,
        zone: TimeZone,
        totals: MutableMap<LocalDate, Long>,
    ) {
        var cursor = interval.startedAt
        while (cursor < interval.endedAt) {
            val date = cursor.toLocalDateTime(zone).date
            val nextDate = date.plus(DatePeriod(days = 1)).atStartOfDayIn(zone)
            val segmentEnd = minOf(interval.endedAt, nextDate)
            val duration = (segmentEnd - cursor).inWholeMilliseconds
            totals[date] = (totals[date] ?: 0L) + duration
            cursor = segmentEnd
        }
    }

    private fun LocalDate.minusOneDay(): LocalDate = plus(DatePeriod(days = -1))
}
