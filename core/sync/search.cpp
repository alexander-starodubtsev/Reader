#include "goodreader/alignment/search.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace goodreader::alignment {
namespace {

[[nodiscard]] std::string join_gram(const std::vector<std::string>& words, std::size_t start,
                                    std::size_t size) {
    std::string gram;
    for (std::size_t index = 0; index < size; ++index) {
        if (index > 0) {
            gram.push_back(' ');
        }
        gram += words[start + index];
    }
    return gram;
}

[[nodiscard]] std::vector<std::string> grams(const std::vector<std::string>& words,
                                             std::size_t gram_size) {
    if (words.size() < gram_size) {
        return {};
    }
    std::vector<std::string> result;
    result.reserve(words.size() - gram_size + 1);
    for (std::size_t start = 0; start + gram_size <= words.size(); ++start) {
        result.push_back(join_gram(words, start, gram_size));
    }
    return result;
}

[[nodiscard]] std::size_t levenshtein_distance(const std::string& left, const std::string& right) {
    std::vector<std::size_t> previous(right.size() + 1);
    for (std::size_t index = 0; index <= right.size(); ++index) {
        previous[index] = index;
    }
    for (std::size_t left_index = 0; left_index < left.size(); ++left_index) {
        std::vector<std::size_t> current(right.size() + 1);
        current[0] = left_index + 1;
        for (std::size_t right_index = 0; right_index < right.size(); ++right_index) {
            const auto substitution =
                previous[right_index] +
                static_cast<std::size_t>(left[left_index] != right[right_index]);
            current[right_index + 1] =
                std::min({current[right_index] + 1, previous[right_index + 1] + 1, substitution});
        }
        previous = std::move(current);
    }
    return previous.back();
}

[[nodiscard]] double word_similarity(const std::string& left, const std::string& right) {
    if (left == right) {
        return 1.0;
    }
    if (left.empty() || right.empty() || std::min(left.size(), right.size()) < 4) {
        return 0.0;
    }
    const auto distance = levenshtein_distance(left, right);
    const auto denominator = std::max(left.size(), right.size());
    return std::max(0.0, 1.0 - static_cast<double>(distance) / static_cast<double>(denominator));
}

[[nodiscard]] double lcs_similarity(const std::vector<std::string>& left,
                                    const std::vector<std::string>& right) {
    std::vector<double> previous(right.size() + 1, 0.0);
    for (const auto& left_word : left) {
        std::vector<double> current(right.size() + 1, 0.0);
        for (std::size_t index = 0; index < right.size(); ++index) {
            const auto match = previous[index] + word_similarity(left_word, right[index]);
            current[index + 1] = std::max({previous[index + 1], current[index], match});
        }
        previous = std::move(current);
    }
    const auto denominator = std::max(left.size(), right.size());
    return std::clamp(previous.back() / static_cast<double>(denominator), 0.0, 1.0);
}

[[nodiscard]] double similarity(const std::vector<std::string>& spoken,
                                const std::vector<std::string>& candidate, std::size_t gram_size) {
    const auto sequence_score = lcs_similarity(spoken, candidate);
    const auto spoken_grams_vector = grams(spoken, gram_size);
    const auto candidate_grams_vector = grams(candidate, gram_size);
    const std::unordered_set<std::string> spoken_grams(spoken_grams_vector.begin(),
                                                       spoken_grams_vector.end());
    const std::unordered_set<std::string> candidate_grams(candidate_grams_vector.begin(),
                                                          candidate_grams_vector.end());
    std::size_t overlap = 0;
    for (const auto& gram : spoken_grams) {
        overlap += static_cast<std::size_t>(candidate_grams.contains(gram));
    }
    const auto denominator = spoken_grams.size() + candidate_grams.size();
    const auto gram_score =
        denominator == 0 ? 1.0
                         : 2.0 * static_cast<double>(overlap) / static_cast<double>(denominator);
    return 0.75 * sequence_score + 0.25 * gram_score;
}

struct RankedMatch {
    std::size_t start;
    double score;
    std::size_t distance_from_estimate;
};

} // namespace

AlignmentSearchConfig::AlignmentSearchConfig(double initial_window_fraction, double left_bias,
                                             double expansion_factor, double minimum_confidence,
                                             double minimum_margin,
                                             std::int64_t fragment_duration_millis)
    : initial_window_fraction(initial_window_fraction), left_bias(left_bias),
      expansion_factor(expansion_factor), minimum_confidence(minimum_confidence),
      minimum_margin(minimum_margin), fragment_duration_millis(fragment_duration_millis) {
    if (this->initial_window_fraction < 0.01 || this->initial_window_fraction > 1.0 ||
        this->left_bias < 0.5 || this->left_bias > 1.0 || this->expansion_factor <= 1.0 ||
        this->minimum_confidence < 0.0 || this->minimum_confidence > 1.0 ||
        this->minimum_margin < 0.0 || this->minimum_margin > 1.0 ||
        this->fragment_duration_millis <= 0) {
        throw std::invalid_argument("Alignment search configuration is invalid.");
    }
}

NGramMatcher::NGramMatcher(std::size_t gram_size) : gram_size_(gram_size) {
    if (gram_size_ < 2) {
        throw std::invalid_argument("N-gram size must be at least two.");
    }
}

std::optional<TextMatch> NGramMatcher::find_best_match(const std::vector<std::string>& book_words,
                                                       const std::vector<std::string>& spoken_words,
                                                       std::size_t window_start,
                                                       std::size_t window_end,
                                                       std::size_t expected_word_offset) const {
    const auto start = std::min(window_start, book_words.size());
    const auto end = std::clamp(window_end, start, book_words.size());
    if (spoken_words.size() < gram_size_ || end - start < gram_size_) {
        return std::nullopt;
    }

    std::map<std::string, std::vector<std::size_t>> query_gram_positions;
    const auto query_grams = grams(spoken_words, gram_size_);
    for (std::size_t index = 0; index < query_grams.size(); ++index) {
        query_gram_positions[query_grams[index]].push_back(index);
    }

    std::set<std::size_t> starts;
    for (std::size_t book_index = start; book_index + gram_size_ <= end; ++book_index) {
        const auto gram = join_gram(book_words, book_index, gram_size_);
        const auto query_matches = query_gram_positions.find(gram);
        if (query_matches == query_gram_positions.end()) {
            continue;
        }
        for (const auto query_index : query_matches->second) {
            const auto candidate_start =
                static_cast<std::ptrdiff_t>(book_index) - static_cast<std::ptrdiff_t>(query_index);
            for (std::ptrdiff_t adjustment = -2; adjustment <= 2; ++adjustment) {
                const auto adjusted_start = candidate_start + adjustment;
                if (adjusted_start >= static_cast<std::ptrdiff_t>(start) &&
                    adjusted_start + static_cast<std::ptrdiff_t>(gram_size_) <=
                        static_cast<std::ptrdiff_t>(end)) {
                    starts.insert(static_cast<std::size_t>(adjusted_start));
                }
            }
        }
    }

    if (starts.empty()) {
        const auto latest_expected =
            end > spoken_words.size() ? std::max(start, end - spoken_words.size()) : start;
        starts.insert(std::clamp(expected_word_offset, start, latest_expected));
        const auto stride = std::max<std::size_t>(1, (end - start) / 128);
        for (auto sample = start; sample + gram_size_ <= end; sample += stride) {
            starts.insert(sample);
        }
    }

    const auto minimum_length = std::max(
        gram_size_, static_cast<std::size_t>(static_cast<double>(spoken_words.size()) * 0.8));
    const auto maximum_length = std::max(
        minimum_length, static_cast<std::size_t>(static_cast<double>(spoken_words.size()) * 1.2));

    std::vector<RankedMatch> ranked;
    for (const auto candidate_start : starts) {
        for (auto length = minimum_length; length <= maximum_length; ++length) {
            if (candidate_start + length > end) {
                continue;
            }
            const std::vector<std::string> candidate(
                book_words.begin() + static_cast<std::ptrdiff_t>(candidate_start),
                book_words.begin() + static_cast<std::ptrdiff_t>(candidate_start + length));
            const auto distance = candidate_start > expected_word_offset
                                      ? candidate_start - expected_word_offset
                                      : expected_word_offset - candidate_start;
            ranked.push_back(
                {candidate_start, similarity(spoken_words, candidate, gram_size_), distance});
        }
    }

    if (ranked.empty()) {
        return std::nullopt;
    }
    std::sort(ranked.begin(), ranked.end(), [](const RankedMatch& left, const RankedMatch& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        if (left.distance_from_estimate != right.distance_from_estimate) {
            return left.distance_from_estimate < right.distance_from_estimate;
        }
        return left.start < right.start;
    });

    const auto best = ranked.front();
    const auto second =
        std::find_if(ranked.begin(), ranked.end(),
                     [&best](const RankedMatch& value) { return value.start != best.start; });
    const auto second_score = second == ranked.end() ? 0.0 : second->score;
    const auto offset = static_cast<std::int32_t>(best.start);
    return TextMatch(domain::WordOffset(offset), std::clamp(best.score, 0.0, 1.0),
                     std::clamp(best.score - second_score, 0.0, 1.0));
}

AdaptiveWindowSearcher::AdaptiveWindowSearcher(NGramMatcher matcher)
    : matcher_(std::move(matcher)) {}

std::optional<TextMatch> AdaptiveWindowSearcher::search(
    const std::vector<std::string>& book_words, const std::vector<std::string>& spoken_words,
    std::int32_t estimated_word_offset, const AlignmentSearchConfig& config) const {
    if (book_words.empty() || spoken_words.empty()) {
        return std::nullopt;
    }
    const auto estimate = static_cast<std::size_t>(
        std::clamp(estimated_word_offset, 0, static_cast<std::int32_t>(book_words.size() - 1)));
    auto fraction = config.initial_window_fraction;
    std::optional<TextMatch> best;
    while (true) {
        const auto window =
            window_for(static_cast<std::int32_t>(book_words.size()),
                       static_cast<std::int32_t>(estimate), fraction, config.left_bias);
        const auto candidate = matcher_.find_best_match(
            book_words, spoken_words, static_cast<std::size_t>(window.start_word),
            static_cast<std::size_t>(window.end_word_exclusive), estimate);
        if (candidate.has_value() &&
            (!best.has_value() || candidate->confidence > best->confidence)) {
            best = candidate;
        }
        if (candidate.has_value() && candidate->confidence >= config.minimum_confidence &&
            candidate->margin >= config.minimum_margin) {
            return candidate;
        }
        if (fraction >= 1.0) {
            return best.has_value() && best->confidence >= config.minimum_confidence ? best
                                                                                     : std::nullopt;
        }
        fraction = std::min(fraction * config.expansion_factor, 1.0);
    }
}

SearchWindow AdaptiveWindowSearcher::window_for(std::int32_t word_count, std::int32_t estimate,
                                                double fraction, double left_bias) const {
    if (word_count < 0 || fraction < 0.0 || fraction > 1.0 || left_bias < 0.5 || left_bias > 1.0) {
        throw std::invalid_argument("Search window arguments are invalid.");
    }
    if (word_count == 0) {
        return {0, 0};
    }

    const auto center = std::clamp(estimate, 0, word_count - 1);
    auto window_size =
        static_cast<std::int32_t>(std::ceil(static_cast<double>(word_count) * fraction));
    window_size = std::clamp(window_size, 1, word_count);
    const auto words_to_left =
        static_cast<std::int32_t>(std::floor(static_cast<double>(window_size) * left_bias));
    auto start = std::max(center - words_to_left, 0);
    auto end = std::min(start + window_size, word_count);
    start = std::max(end - window_size, 0);
    end = std::min(start + window_size, word_count);
    return {start, end};
}

} // namespace goodreader::alignment
