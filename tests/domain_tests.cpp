#include "goodreader/domain/alignment.hpp"
#include "goodreader/domain/types.hpp"

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace goodreader::tests {
namespace {

using domain::ActiveReadingInterval;
using domain::AlignmentAnchor;
using domain::AlignmentIndex;
using domain::AudioFormat;
using domain::AudioTrack;
using domain::AudioTrackId;
using domain::Book;
using domain::BookFormat;
using domain::BookId;
using domain::ListeningProgress;
using domain::LocalDate;
using domain::Milliseconds;
using domain::ReadingProgress;
using domain::ReadingSession;
using domain::StreakCalculator;
using domain::StreakPolicy;
using domain::Timestamp;
using domain::WordOffset;

TEST(DomainBookTests, ParsesSupportedFormatsWithoutCaseSensitivity) {
    EXPECT_EQ(domain::book_format_from_extension(".Epub"), BookFormat::epub);
    EXPECT_EQ(domain::book_format_from_extension("FB2"), BookFormat::fb2);
    EXPECT_EQ(domain::audio_format_from_extension("m4b"), AudioFormat::m4b);
    EXPECT_EQ(domain::audio_format_from_extension(".WAV"), AudioFormat::wav);
    EXPECT_EQ(domain::book_format_from_extension("txt"), std::nullopt);
    EXPECT_EQ(domain::audio_format_from_extension("flac"), std::nullopt);
}

TEST(DomainBookTests, RejectsInvalidBookAndTrackMetadata) {
    EXPECT_THROW(BookId(" "), std::invalid_argument);
    EXPECT_THROW(AudioTrackId(""), std::invalid_argument);
    EXPECT_THROW(Book(BookId("book"), " ", std::nullopt, BookFormat::pdf, "book.pdf", std::nullopt),
                 std::invalid_argument);
    EXPECT_THROW(Book(BookId("book"), "Title", std::nullopt, BookFormat::pdf, "book.pdf", -1),
                 std::invalid_argument);
    EXPECT_THROW(Book(BookId("book"), "Title", std::nullopt, BookFormat::pdf, " ", 10),
                 std::invalid_argument);
    EXPECT_THROW(AudioTrack(AudioTrackId("audio"), BookId("book"), "", AudioFormat::mp3, 1, 0),
                 std::invalid_argument);
    EXPECT_THROW(
        AudioTrack(AudioTrackId("audio"), BookId("book"), "track.m4b", AudioFormat::m4b, 0, 0),
        std::invalid_argument);
    EXPECT_THROW(
        AudioTrack(AudioTrackId("audio"), BookId("book"), "track.m4b", AudioFormat::m4b, 10, -1),
        std::invalid_argument);
}

TEST(DomainProgressTests, RejectsNegativePositions) {
    const auto timestamp = Timestamp{Milliseconds{0}};
    EXPECT_THROW(ReadingProgress(BookId("book"), -1, timestamp), std::invalid_argument);
    EXPECT_THROW(ListeningProgress(BookId("book"), AudioTrackId("track"), -1, timestamp),
                 std::invalid_argument);
    EXPECT_THROW(WordOffset(-1), std::invalid_argument);
}

TEST(DomainAlignmentIndexTests, InterpolatesBothDirectionsAndClampsAtEdges) {
    const AlignmentIndex index(
        {AlignmentAnchor(WordOffset(0), 0, 1.0), AlignmentAnchor(WordOffset(100), 200'000, 0.9)});

    ASSERT_EQ(index.word_at_audio_time(100'000), WordOffset(50));
    ASSERT_EQ(index.audio_time_at_word(WordOffset(50)), 100'000);
    ASSERT_EQ(index.word_at_audio_time(-1), WordOffset(0));
    ASSERT_EQ(index.word_at_audio_time(300'000), WordOffset(100));
    ASSERT_EQ(index.audio_time_at_word(WordOffset(150)), 200'000);
    ASSERT_EQ(index.confidence_at_audio_time(100'000), 0.9);
    EXPECT_FALSE(index.confidence_at_audio_time(-1).has_value());
}

TEST(DomainAlignmentIndexTests, ReturnsNoEstimateWithoutAnchors) {
    const AlignmentIndex index({});
    EXPECT_FALSE(index.word_at_audio_time(1).has_value());
    EXPECT_FALSE(index.audio_time_at_word(WordOffset(1)).has_value());
    EXPECT_FALSE(index.confidence_at_audio_time(1).has_value());
}

TEST(DomainAlignmentIndexTests, RejectsInvalidAndNonMonotonicAnchors) {
    EXPECT_THROW(AlignmentAnchor(WordOffset(0), -1, 0.5), std::invalid_argument);
    EXPECT_THROW(AlignmentAnchor(WordOffset(0), 0, 1.1), std::invalid_argument);
    EXPECT_THROW(AlignmentAnchor(WordOffset(0), 0, -0.1), std::invalid_argument);
    EXPECT_THROW(AlignmentIndex({AlignmentAnchor(WordOffset(5), 10, 0.9),
                                 AlignmentAnchor(WordOffset(4), 20, 0.9)}),
                 std::invalid_argument);
    EXPECT_THROW(AlignmentIndex({AlignmentAnchor(WordOffset(4), 20, 0.9),
                                 AlignmentAnchor(WordOffset(5), 10, 0.9)}),
                 std::invalid_argument);
}

TEST(DomainStreakTests, SplitsAtMidnightAndRequiresTenMinutesOnEachDate) {
    const ReadingSession session("midnight", BookId("book"), "UTC",
                                 {ActiveReadingInterval(parse_timestamp("2025-01-01T23:55:00Z"),
                                                        parse_timestamp("2025-01-02T00:15:00Z"))});

    EXPECT_EQ(StreakCalculator::qualifying_dates({session}),
              (std::set<LocalDate>{LocalDate(2025, 1, 2)}));
    EXPECT_EQ(session.active_millis(), 20 * 60 * 1000);
}

TEST(DomainStreakTests, AggregatesSeparatedIntervalsAndValidatesSessionRules) {
    const std::vector<ReadingSession> sessions{
        ReadingSession("first", BookId("book"), "UTC",
                       {ActiveReadingInterval(parse_timestamp("2025-01-03T10:00:00Z"),
                                              parse_timestamp("2025-01-03T10:05:00Z"))}),
        ReadingSession("second", BookId("book"), "UTC",
                       {ActiveReadingInterval(parse_timestamp("2025-01-03T11:00:00Z"),
                                              parse_timestamp("2025-01-03T11:05:00Z"))})};

    EXPECT_EQ(StreakCalculator::qualifying_dates(sessions),
              (std::set<LocalDate>{LocalDate(2025, 1, 3)}));
    EXPECT_TRUE(StreakCalculator::qualifying_dates({}).empty());
    EXPECT_THROW(StreakPolicy(0), std::invalid_argument);
    EXPECT_THROW(ActiveReadingInterval(parse_timestamp("2025-01-03T10:00:00Z"),
                                       parse_timestamp("2025-01-03T10:00:00Z")),
                 std::invalid_argument);
    EXPECT_THROW(ReadingSession(" ", BookId("book"), "UTC", {}), std::invalid_argument);
    EXPECT_THROW(ReadingSession("overlap", BookId("book"), "UTC",
                                {ActiveReadingInterval(parse_timestamp("2025-01-03T10:00:00Z"),
                                                       parse_timestamp("2025-01-03T10:10:00Z")),
                                 ActiveReadingInterval(parse_timestamp("2025-01-03T10:05:00Z"),
                                                       parse_timestamp("2025-01-03T10:15:00Z"))}),
                 std::invalid_argument);
}

TEST(DomainStreakTests, UsesLocalCalendarDaysAcrossDaylightSavingTransitions) {
    const ReadingSession session("spring-forward", BookId("book"), "America/New_York",
                                 {ActiveReadingInterval(parse_timestamp("2025-03-09T06:50:00Z"),
                                                        parse_timestamp("2025-03-09T07:10:00Z"))});

    EXPECT_EQ(StreakCalculator::qualifying_dates({session}),
              (std::set<LocalDate>{LocalDate(2025, 3, 9)}));
}

TEST(DomainStreakTests, KeepsYesterdayUntilTodayQualifiesAndResetsAfterAMissedDate) {
    EXPECT_EQ(StreakCalculator::current_streak({LocalDate(2025, 2, 1), LocalDate(2025, 2, 2)},
                                               LocalDate(2025, 2, 3)),
              2);
    EXPECT_EQ(StreakCalculator::current_streak({LocalDate(2025, 2, 1)}, LocalDate(2025, 2, 3)), 0);
    EXPECT_EQ(StreakCalculator::current_streak(
                  {LocalDate(2025, 3, 8), LocalDate(2025, 3, 9), LocalDate(2025, 3, 10)},
                  LocalDate(2025, 3, 10)),
              3);
}

TEST(DomainStreakTests, ValidatesCalendarDatesAndReportsUnknownTimeZones) {
    EXPECT_THROW(LocalDate(2025, 2, 29), std::invalid_argument);
    EXPECT_EQ(LocalDate(2024, 2, 28).plus_days(1), LocalDate(2024, 2, 29));
    const ReadingSession invalid_zone(
        "invalid-zone", BookId("book"), "Invalid/TimeZone",
        {ActiveReadingInterval(parse_timestamp("2025-01-01T10:00:00Z"),
                               parse_timestamp("2025-01-01T10:15:00Z"))});
    EXPECT_THROW(StreakCalculator::qualifying_dates({invalid_zone}), std::runtime_error);
}

} // namespace
} // namespace goodreader::tests
