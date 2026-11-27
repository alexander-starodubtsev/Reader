#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace goodreader::domain {

using Milliseconds = std::chrono::milliseconds;
using Timestamp = std::chrono::sys_time<Milliseconds>;

/** Stable identifier for a book stored in the local library. */
struct BookId {
    std::string value;

    /** Creates a non-blank book identifier. */
    explicit BookId(std::string value);
    bool operator==(const BookId&) const = default;
};

/** Supported text file formats. */
enum class BookFormat { pdf, epub, fb2 };

/** Returns a supported format for a file extension, or null when unsupported. */
std::optional<BookFormat> book_format_from_extension(std::string extension);

/** A locally imported book and its user-visible metadata. */
struct Book {
    BookId id;
    std::string title;
    std::optional<std::string> author;
    BookFormat format;
    std::string file_uri;
    std::optional<std::int32_t> word_count;

    /** Creates a book with a non-blank title and URI and a non-negative optional word count. */
    Book(BookId id, std::string title, std::optional<std::string> author, BookFormat format,
         std::string file_uri, std::optional<std::int32_t> word_count);
    bool operator==(const Book&) const = default;
};

/** Stable identifier for an audio file associated with a book. */
struct AudioTrackId {
    std::string value;

    /** Creates a non-blank audio track identifier. */
    explicit AudioTrackId(std::string value);
    bool operator==(const AudioTrackId&) const = default;
};

/** Supported audio file formats. */
enum class AudioFormat { mp3, m4a, m4b, ogg, wav };

/** Returns a supported format for a file extension, or null when unsupported. */
std::optional<AudioFormat> audio_format_from_extension(std::string extension);

/** An audio file in a book's ordered audiobook, with duration in milliseconds. */
struct AudioTrack {
    AudioTrackId id;
    BookId book_id;
    std::string uri;
    AudioFormat format;
    std::int64_t duration_millis;
    std::int32_t chapter_index;

    /** Creates a track with a non-blank URI, positive duration, and non-negative chapter index. */
    AudioTrack(AudioTrackId id, BookId book_id, std::string uri, AudioFormat format,
               std::int64_t duration_millis, std::int32_t chapter_index);
    bool operator==(const AudioTrack&) const = default;
};

/** Last saved text position for a book, as a zero-based character offset. */
struct ReadingProgress {
    BookId book_id;
    std::int64_t character_offset;
    Timestamp updated_at;

    /** Creates a progress value with a non-negative character offset. */
    ReadingProgress(BookId book_id, std::int64_t character_offset, Timestamp updated_at);
    bool operator==(const ReadingProgress&) const = default;
};

/** Last saved audio position in a specific track. */
struct ListeningProgress {
    BookId book_id;
    AudioTrackId track_id;
    std::int64_t position_millis;
    Timestamp updated_at;

    /** Creates a progress value with a non-negative audio position. */
    ListeningProgress(BookId book_id, AudioTrackId track_id, std::int64_t position_millis,
                      Timestamp updated_at);
    bool operator==(const ListeningProgress&) const = default;
};

/** A word offset in normalized book text. */
struct WordOffset {
    std::int32_t value;

    /** Creates a non-negative normalized word offset. */
    explicit WordOffset(std::int32_t value);
    bool operator==(const WordOffset&) const = default;
};

/** A monotonic correspondence between a text word and an audio timestamp. */
struct AlignmentAnchor {
    WordOffset word_offset;
    std::int64_t audio_time_millis;
    double confidence;

    /** Creates an anchor with valid timestamp and confidence values. */
    AlignmentAnchor(WordOffset word_offset, std::int64_t audio_time_millis, double confidence);
    bool operator==(const AlignmentAnchor&) const = default;
};

/** Cached anchors linking audiobook time to a word position in the book. */
struct AlignmentIndex {
    std::vector<AlignmentAnchor> anchors;

    /** Creates an index whose anchors increase strictly in text and audio time. */
    explicit AlignmentIndex(std::vector<AlignmentAnchor> anchors);
    /** Estimates a word offset at the specified audio time, clamping to the index bounds. */
    std::optional<WordOffset> word_at_audio_time(std::int64_t audio_time_millis) const;
    /** Estimates the audio time at the specified word offset, clamping to the index bounds. */
    std::optional<std::int64_t> audio_time_at_word(WordOffset word_offset) const;
    /** Returns the exact or bracketed anchor confidence, or null outside the index. */
    std::optional<double> confidence_at_audio_time(std::int64_t audio_time_millis) const;
    bool operator==(const AlignmentIndex&) const = default;
};

/** A half-open interval of active reading time. */
struct ActiveReadingInterval {
    Timestamp started_at;
    Timestamp ended_at;

    /** Creates a half-open interval with positive duration. */
    ActiveReadingInterval(Timestamp started_at, Timestamp ended_at);
    bool operator==(const ActiveReadingInterval&) const = default;
};

/** A recorded reading session with active intervals and the time zone used during it. */
struct ReadingSession {
    std::string id;
    BookId book_id;
    std::string time_zone_id;
    std::vector<ActiveReadingInterval> active_intervals;

    /** Creates an ordered, non-overlapping set of active intervals for one local time zone. */
    ReadingSession(std::string id, BookId book_id, std::string time_zone_id,
                   std::vector<ActiveReadingInterval> active_intervals);
    /** Returns the total active duration in milliseconds. */
    std::int64_t active_millis() const;
};

/** Rules for converting active reading intervals into qualifying calendar days. */
struct StreakPolicy {
    std::int64_t minimum_active_millis_per_day = 10 * 60 * 1000;

    /** Creates a positive daily reading threshold in milliseconds. */
    explicit StreakPolicy(std::int64_t minimum_active_millis_per_day = 10 * 60 * 1000);
    bool operator==(const StreakPolicy&) const = default;
};

/** A validated local calendar date independent of any platform UI type. */
struct LocalDate {
    int year;
    unsigned month;
    unsigned day;

    /** Creates a valid Gregorian calendar date. */
    LocalDate(int year, unsigned month, unsigned day);
    /** Returns this calendar date shifted by a signed number of calendar days. */
    LocalDate plus_days(int days) const;
    bool operator==(const LocalDate&) const = default;
    auto operator<=>(const LocalDate&) const = default;
};

/** Calculates qualifying local dates and consecutive reading streaks. */
class StreakCalculator {
  public:
    /** Returns dates on which active reading met the daily threshold. */
    static std::set<LocalDate> qualifying_dates(const std::vector<ReadingSession>& sessions,
                                                StreakPolicy policy = StreakPolicy{});

    /**
     * Returns the streak ending today, or yesterday when today has not yet qualified.
     * A missed calendar day resets the count.
     */
    static std::size_t current_streak(const std::set<LocalDate>& qualifying_dates, LocalDate today);
};

} // namespace goodreader::domain
