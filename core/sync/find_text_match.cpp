#include "goodreader/application/find_text_match.hpp"

#include "goodreader/alignment/text_normalizer.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace goodreader::application {
namespace {

[[nodiscard]] bool is_blank(const std::string& value) {
    return value.find_first_not_of(" \t\n\r\f\v") == std::string::npos;
}

} // namespace

FindTextMatchUseCase::FindTextMatchUseCase(SpeechRecognizer& speech_recognizer,
                                           alignment::AdaptiveWindowSearcher searcher)
    : speech_recognizer_(speech_recognizer), searcher_(std::move(searcher)) {}

AppResult<AudioTextMatch> FindTextMatchUseCase::operator()(
    const std::string& book_text, const std::string& audio_uri, std::int64_t audio_position_millis,
    std::int64_t total_audio_duration_millis, const alignment::AlignmentSearchConfig& config) {
    if (is_blank(audio_uri)) {
        return AppResult<AudioTextMatch>::failure(
            ValidationError{"audio_uri_required", "Choose an audio file."});
    }
    if (audio_position_millis < 0 || total_audio_duration_millis <= 0 ||
        audio_position_millis > total_audio_duration_millis) {
        return AppResult<AudioTextMatch>::failure(
            ValidationError{"invalid_audio_position", "Audio position must be within the track."});
    }

    const auto book_words = alignment::TextNormalizer::tokenize(book_text);
    if (book_words.empty()) {
        return AppResult<AudioTextMatch>::failure(
            ValidationError{"empty_book_text", "The book contains no searchable text."});
    }

    const auto available_fragment =
        audio_position_millis == 0 ? total_audio_duration_millis : audio_position_millis;
    const auto fragment_duration = std::min(config.fragment_duration_millis, available_fragment);
    const auto fragment_start =
        std::max<std::int64_t>(0, audio_position_millis - fragment_duration);
    auto transcription =
        speech_recognizer_.transcribe(audio_uri, fragment_start, fragment_duration);
    if (!transcription.has_value()) {
        return AppResult<AudioTextMatch>::failure(transcription.error());
    }
    const auto recognized = transcription.value();
    const auto spoken_words = alignment::TextNormalizer::tokenize(recognized.text);
    if (spoken_words.empty()) {
        return AppResult<AudioTextMatch>::failure(
            ValidationError{"empty_transcript", "Speech recognition returned no words."});
    }

    const auto ratio = static_cast<double>(audio_position_millis) /
                       static_cast<double>(total_audio_duration_millis);
    const auto estimated_offset =
        static_cast<std::int32_t>(ratio * static_cast<double>(book_words.size()));
    const auto match = searcher_.search(book_words, spoken_words, estimated_offset, config);
    if (!match.has_value()) {
        return AppResult<AudioTextMatch>::failure(
            UnavailableError{"alignment", "No confident text match was found."});
    }
    return AppResult<AudioTextMatch>::success(AudioTextMatch{*match, recognized, fragment_start});
}

} // namespace goodreader::application
