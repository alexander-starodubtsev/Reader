package com.goodreader.domain

import kotlinx.datetime.Instant
import kotlinx.datetime.LocalDate
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class StreakCalculatorTest {
    @Test
    fun splitsReadingAcrossMidnightAndRequiresTenMinutesPerDate() {
        val session =
            session(
                "2025-01-01T23:55:00Z",
                "2025-01-02T00:15:00Z",
                zone = "UTC",
            )

        assertEquals(
            setOf(LocalDate(2025, 1, 2)),
            StreakCalculator.qualifyingDates(listOf(session)),
        )
    }

    @Test
    fun calculatesConsecutiveDatesWithoutAssumingTwentyFourHourDays() {
        val qualifying =
            setOf(
                LocalDate(2025, 3, 8),
                LocalDate(2025, 3, 9),
                LocalDate(2025, 3, 10),
            )

        assertEquals(3, StreakCalculator.currentStreak(qualifying, LocalDate(2025, 3, 10)))
    }

    @Test
    fun preservesYesterdayUntilTodayQualifiesAndResetsAfterMissedDay() {
        assertEquals(
            2,
            StreakCalculator.currentStreak(
                setOf(LocalDate(2025, 2, 1), LocalDate(2025, 2, 2)),
                LocalDate(2025, 2, 3),
            ),
        )
        assertEquals(
            0,
            StreakCalculator.currentStreak(
                setOf(LocalDate(2025, 2, 1)),
                LocalDate(2025, 2, 3),
            ),
        )
    }

    @Test
    fun usesLocalCalendarBoundariesDuringDstTransition() {
        val session =
            session(
                "2025-03-09T06:50:00Z",
                "2025-03-09T07:10:00Z",
                zone = "America/New_York",
            )

        assertEquals(
            setOf(LocalDate(2025, 3, 9)),
            StreakCalculator.qualifyingDates(listOf(session)),
        )
    }

    @Test
    fun aggregatesSeparatedActiveIntervalsAndValidatesSessionRules() {
        val sessions =
            listOf(
                ReadingSession(
                    id = "first",
                    bookId = BookId("book"),
                    timeZoneId = "UTC",
                    activeIntervals =
                        listOf(
                            ActiveReadingInterval(Instant.parse("2025-01-03T10:00:00Z"), Instant.parse("2025-01-03T10:05:00Z")),
                        ),
                ),
                ReadingSession(
                    id = "second",
                    bookId = BookId("book"),
                    timeZoneId = "UTC",
                    activeIntervals =
                        listOf(
                            ActiveReadingInterval(Instant.parse("2025-01-03T11:00:00Z"), Instant.parse("2025-01-03T11:05:00Z")),
                        ),
                ),
            )

        assertEquals(setOf(LocalDate(2025, 1, 3)), StreakCalculator.qualifyingDates(sessions))
        assertEquals(emptySet(), StreakCalculator.qualifyingDates(emptyList()))
        assertFailsWith<IllegalArgumentException> { StreakPolicy(0) }
        assertFailsWith<IllegalArgumentException> {
            ActiveReadingInterval(Instant.parse("2025-01-03T10:00:00Z"), Instant.parse("2025-01-03T10:00:00Z"))
        }
        assertFailsWith<IllegalArgumentException> {
            ReadingSession(" ", BookId("book"), "UTC", emptyList())
        }
        assertFailsWith<IllegalArgumentException> {
            ReadingSession(
                "overlap",
                BookId("book"),
                "UTC",
                listOf(
                    ActiveReadingInterval(Instant.parse("2025-01-03T10:00:00Z"), Instant.parse("2025-01-03T10:10:00Z")),
                    ActiveReadingInterval(Instant.parse("2025-01-03T10:05:00Z"), Instant.parse("2025-01-03T10:15:00Z")),
                ),
            )
        }
    }

    private fun session(
        start: String,
        end: String,
        zone: String,
    ) = ReadingSession(
        id = "session",
        bookId = BookId("book"),
        timeZoneId = zone,
        activeIntervals =
            listOf(
                ActiveReadingInterval(Instant.parse(start), Instant.parse(end)),
            ),
    )
}
