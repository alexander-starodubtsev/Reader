#pragma once

#include "goodreader/alignment/search.hpp"
#include "goodreader/application/ports.hpp"

namespace goodreader::application {

/** Audio-derived text location and the evidence used to find it. */
struct AudioTextMatch {
    alignment::TextMatch match;
    RecognizedSpeech transcript;
    std::int64_t fragment_start_millis;
    bool operator==(const AudioTextMatch&) const = default;
};

/** Transcribes a bounded fragment and searches an adaptive book-text window. */
class FindTextMatchUseCase {
  public:
    /** Creates a use case with an on-device recognizer and adaptive searcher. */
    explicit FindTextMatchUseCase(
        SpeechRecognizer& speech_recognizer,
        alignment::AdaptiveWindowSearcher searcher = alignment::AdaptiveWindowSearcher{});

    /** Matches a bounded ASR fragment against the ratio-estimated book-text window. */
    AppResult<AudioTextMatch>
    operator()(const std::string& book_text, const std::string& audio_uri,
               std::int64_t audio_position_millis, std::int64_t total_audio_duration_millis,
               const alignment::AlignmentSearchConfig& config = alignment::AlignmentSearchConfig{});

  private:
    SpeechRecognizer& speech_recognizer_;
    alignment::AdaptiveWindowSearcher searcher_;
};

} // namespace goodreader::application
