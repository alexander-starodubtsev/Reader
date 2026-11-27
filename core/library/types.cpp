#include "goodreader/domain/types.hpp"
#include "goodreader/domain/alignment.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace goodreader::domain {
namespace {

[[nodiscard]] std::string normalized_extension(std::string extension) {
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension;
}

void require_non_blank(const std::string& value, const char* message) {
    if (value.find_first_not_of(" \t\n\r\f\v") == std::string::npos) {
        throw std::invalid_argument(message);
    }
}

} // namespace

BookId::BookId(std::string value) : value(std::move(value)) {
    require_non_blank(this->value, "Book ID must not be blank.");
}

std::optional<BookFormat> book_format_from_extension(std::string extension) {
    const auto normalized = normalized_extension(std::move(extension));
    if (normalized == "pdf") {
        return BookFormat::pdf;
    }
    if (normalized == "epub") {
        return BookFormat::epub;
    }
    if (normalized == "fb2") {
        return BookFormat::fb2;
    }
    return std::nullopt;
}

Book::Book(BookId id, std::string title, std::optional<std::string> author, BookFormat format,
           std::string file_uri, std::optional<std::int32_t> word_count)
    : id(std::move(id)), title(std::move(title)), author(std::move(author)), format(format),
      file_uri(std::move(file_uri)), word_count(word_count) {
    require_non_blank(this->title, "Book title must not be blank.");
    require_non_blank(this->file_uri, "Book file URI must not be blank.");
    if (this->word_count.has_value() && *this->word_count < 0) {
        throw std::invalid_argument("Word count must not be negative.");
    }
}

AudioTrackId::AudioTrackId(std::string value) : value(std::move(value)) {
    require_non_blank(this->value, "Audio track ID must not be blank.");
}

std::optional<AudioFormat> audio_format_from_extension(std::string extension) {
    const auto normalized = normalized_extension(std::move(extension));
    if (normalized == "mp3") {
        return AudioFormat::mp3;
    }
    if (normalized == "m4a") {
        return AudioFormat::m4a;
    }
    if (normalized == "m4b") {
        return AudioFormat::m4b;
    }
    if (normalized == "ogg") {
        return AudioFormat::ogg;
    }
    if (normalized == "wav") {
        return AudioFormat::wav;
    }
    return std::nullopt;
}

AudioTrack::AudioTrack(AudioTrackId id, BookId book_id, std::string uri, AudioFormat format,
                       std::int64_t duration_millis, std::int32_t chapter_index)
    : id(std::move(id)), book_id(std::move(book_id)), uri(std::move(uri)), format(format),
      duration_millis(duration_millis), chapter_index(chapter_index) {
    require_non_blank(this->uri, "Audio URI must not be blank.");
    if (this->duration_millis <= 0) {
        throw std::invalid_argument("Audio duration must be positive.");
    }
    if (this->chapter_index < 0) {
        throw std::invalid_argument("Chapter index must not be negative.");
    }
}

ReadingProgress::ReadingProgress(BookId book_id, std::int64_t character_offset,
                                 Timestamp updated_at)
    : book_id(std::move(book_id)), character_offset(character_offset), updated_at(updated_at) {
    if (this->character_offset < 0) {
        throw std::invalid_argument("Character offset must not be negative.");
    }
}

ListeningProgress::ListeningProgress(BookId book_id, AudioTrackId track_id,
                                     std::int64_t position_millis, Timestamp updated_at)
    : book_id(std::move(book_id)), track_id(std::move(track_id)), position_millis(position_millis),
      updated_at(updated_at) {
    if (this->position_millis < 0) {
        throw std::invalid_argument("Audio position must not be negative.");
    }
}

WordOffset::WordOffset(std::int32_t value) : value(value) {
    if (this->value < 0) {
        throw std::invalid_argument("Word offset must not be negative.");
    }
}

AlignmentAnchor::AlignmentAnchor(WordOffset word_offset, std::int64_t audio_time_millis,
                                 double confidence)
    : word_offset(word_offset), audio_time_millis(audio_time_millis), confidence(confidence) {
    if (this->audio_time_millis < 0) {
        throw std::invalid_argument("Audio timestamp must not be negative.");
    }
    if (this->confidence < 0.0 || this->confidence > 1.0) {
        throw std::invalid_argument("Confidence must be between zero and one.");
    }
}

AlignmentIndex::AlignmentIndex(std::vector<AlignmentAnchor> anchors) : anchors(std::move(anchors)) {
    for (std::size_t index = 1; index < this->anchors.size(); ++index) {
        const auto& left = this->anchors[index - 1];
        const auto& right = this->anchors[index];
        if (left.word_offset.value >= right.word_offset.value ||
            left.audio_time_millis >= right.audio_time_millis) {
            throw std::invalid_argument(
                "Alignment anchors must increase strictly in both text and audio time.");
        }
    }
}

std::optional<WordOffset> AlignmentIndex::word_at_audio_time(std::int64_t audio_time_millis) const {
    if (anchors.empty()) {
        return std::nullopt;
    }
    if (audio_time_millis <= anchors.front().audio_time_millis) {
        return anchors.front().word_offset;
    }
    if (audio_time_millis >= anchors.back().audio_time_millis) {
        return anchors.back().word_offset;
    }

    const auto right = std::lower_bound(anchors.begin(), anchors.end(), audio_time_millis,
                                        [](const AlignmentAnchor& anchor, std::int64_t time) {
                                            return anchor.audio_time_millis < time;
                                        });
    const auto& left_anchor = *(right - 1);
    const auto& right_anchor = *right;
    const auto fraction =
        static_cast<double>(audio_time_millis - left_anchor.audio_time_millis) /
        static_cast<double>(right_anchor.audio_time_millis - left_anchor.audio_time_millis);
    const auto word_delta =
        static_cast<double>(right_anchor.word_offset.value - left_anchor.word_offset.value);
    const auto interpolated =
        static_cast<double>(left_anchor.word_offset.value) + fraction * word_delta;
    return WordOffset(static_cast<std::int32_t>(interpolated));
}

std::optional<std::int64_t> AlignmentIndex::audio_time_at_word(WordOffset word_offset) const {
    if (anchors.empty()) {
        return std::nullopt;
    }
    if (word_offset.value <= anchors.front().word_offset.value) {
        return anchors.front().audio_time_millis;
    }
    if (word_offset.value >= anchors.back().word_offset.value) {
        return anchors.back().audio_time_millis;
    }

    const auto right = std::lower_bound(anchors.begin(), anchors.end(), word_offset.value,
                                        [](const AlignmentAnchor& anchor, std::int32_t word) {
                                            return anchor.word_offset.value < word;
                                        });
    const auto& left_anchor = *(right - 1);
    const auto& right_anchor = *right;
    const auto fraction =
        static_cast<double>(word_offset.value - left_anchor.word_offset.value) /
        static_cast<double>(right_anchor.word_offset.value - left_anchor.word_offset.value);
    const auto time_delta =
        static_cast<double>(right_anchor.audio_time_millis - left_anchor.audio_time_millis);
    return left_anchor.audio_time_millis + static_cast<std::int64_t>(fraction * time_delta);
}

std::optional<double>
AlignmentIndex::confidence_at_audio_time(std::int64_t audio_time_millis) const {
    if (anchors.empty() || audio_time_millis < anchors.front().audio_time_millis ||
        audio_time_millis > anchors.back().audio_time_millis) {
        return std::nullopt;
    }
    if (audio_time_millis == anchors.front().audio_time_millis) {
        return anchors.front().confidence;
    }
    if (audio_time_millis == anchors.back().audio_time_millis) {
        return anchors.back().confidence;
    }

    const auto right = std::lower_bound(anchors.begin(), anchors.end(), audio_time_millis,
                                        [](const AlignmentAnchor& anchor, std::int64_t time) {
                                            return anchor.audio_time_millis < time;
                                        });
    return std::min((right - 1)->confidence, right->confidence);
}

ActiveReadingInterval::ActiveReadingInterval(Timestamp started_at, Timestamp ended_at)
    : started_at(started_at), ended_at(ended_at) {
    if (this->ended_at <= this->started_at) {
        throw std::invalid_argument("An active interval must have positive duration.");
    }
}

ReadingSession::ReadingSession(std::string id, BookId book_id, std::string time_zone_id,
                               std::vector<ActiveReadingInterval> active_intervals)
    : id(std::move(id)), book_id(std::move(book_id)), time_zone_id(std::move(time_zone_id)),
      active_intervals(std::move(active_intervals)) {
    require_non_blank(this->id, "Reading session ID must not be blank.");
    require_non_blank(this->time_zone_id, "Time-zone ID must not be blank.");
    for (std::size_t index = 1; index < this->active_intervals.size(); ++index) {
        if (this->active_intervals[index - 1].ended_at > this->active_intervals[index].started_at) {
            throw std::invalid_argument(
                "Active reading intervals must be ordered and non-overlapping.");
        }
    }
}

std::int64_t ReadingSession::active_millis() const {
    std::int64_t total = 0;
    for (const auto& interval : active_intervals) {
        total += (interval.ended_at - interval.started_at).count();
    }
    return total;
}

StreakPolicy::StreakPolicy(std::int64_t minimum_active_millis_per_day)
    : minimum_active_millis_per_day(minimum_active_millis_per_day) {
    if (this->minimum_active_millis_per_day <= 0) {
        throw std::invalid_argument("Daily reading threshold must be positive.");
    }
}

AlignmentMatch::AlignmentMatch(WordOffset word_offset, double confidence, double margin)
    : word_offset(word_offset), confidence(confidence), margin(margin) {
    if (this->confidence < 0.0 || this->confidence > 1.0 || this->margin < 0.0 ||
        this->margin > 1.0) {
        throw std::invalid_argument(
            "Alignment confidence and margin must be between zero and one.");
    }
}

} // namespace goodreader::domain
