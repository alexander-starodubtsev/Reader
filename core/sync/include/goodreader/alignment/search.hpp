#pragma once

#include "goodreader/domain/alignment.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace goodreader::alignment {

using TextMatch = domain::AlignmentMatch;

/** Tunable limits for alignment search and audio-fragment transcription. */
struct AlignmentSearchConfig {
    double initial_window_fraction = 0.20;
    double left_bias = 0.65;
    double expansion_factor = 1.8;
    double minimum_confidence = 0.62;
    double minimum_margin = 0.05;
    std::int64_t fragment_duration_millis = 25'000;

    /** Creates validated search, confidence, and audio-fragment parameters. */
    AlignmentSearchConfig(double initial_window_fraction = 0.20, double left_bias = 0.65,
                          double expansion_factor = 1.8, double minimum_confidence = 0.62,
                          double minimum_margin = 0.05,
                          std::int64_t fragment_duration_millis = 25'000);
};

/** The exact text interval considered during one search attempt. */
struct SearchWindow {
    std::int32_t start_word;
    std::int32_t end_word_exclusive;

    bool operator==(const SearchWindow&) const = default;
};

/** Finds approximate ordered phrase matches in a bounded range of book words. */
class NGramMatcher {
  public:
    /** Creates a matcher with an n-gram size of at least two tokens. */
    explicit NGramMatcher(std::size_t gram_size = 3);

    /** Matches a transcript against [window_start, window_end) only. */
    std::optional<TextMatch> find_best_match(const std::vector<std::string>& book_words,
                                             const std::vector<std::string>& spoken_words,
                                             std::size_t window_start, std::size_t window_end,
                                             std::size_t expected_word_offset) const;

  private:
    std::size_t gram_size_;
};

/** Expands a left-biased candidate range around a ratio-based position estimate. */
class AdaptiveWindowSearcher {
  public:
    /** Creates a searcher using the provided n-gram matcher. */
    explicit AdaptiveWindowSearcher(NGramMatcher matcher = NGramMatcher{});

    /** Returns the best confident match, expanding the window only as needed. */
    std::optional<TextMatch>
    search(const std::vector<std::string>& book_words, const std::vector<std::string>& spoken_words,
           std::int32_t estimated_word_offset,
           const AlignmentSearchConfig& config = AlignmentSearchConfig{}) const;

    /** Calculates a bounded text window, assigning more width to the left side. */
    SearchWindow window_for(std::int32_t word_count, std::int32_t estimate, double fraction,
                            double left_bias = 0.65) const;

  private:
    NGramMatcher matcher_;
};

} // namespace goodreader::alignment
