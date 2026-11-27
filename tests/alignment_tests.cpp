#include "goodreader/alignment/search.hpp"
#include "goodreader/alignment/text_normalizer.hpp"

#include "test_support.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace goodreader::tests {
namespace {

using alignment::AdaptiveWindowSearcher;
using alignment::AlignmentSearchConfig;
using alignment::NGramMatcher;
using alignment::TextNormalizer;

TEST(TextNormalizerTests, FoldsPunctuationCaseRussianYoAndDiacritics) {
    EXPECT_EQ(TextNormalizer::tokenize("ЕЩЁ! Café, 1,000 слов"),
              (std::vector<std::string>{"еще", "cafe", "1000", "слов"}));
}

TEST(TextNormalizerTests, PreservesNonLatinLettersAndSeparatesPunctuation) {
    EXPECT_EQ(TextNormalizer::tokenize("Привет, мир! Καλημέρα."),
              (std::vector<std::string>{"привет", "мир", "καλημερα"}));
}

TEST(AlignmentConfigTests, RejectsInvalidSearchParametersAndNGramSizes) {
    EXPECT_THROW(AlignmentSearchConfig(0.0), std::invalid_argument);
    EXPECT_THROW(AlignmentSearchConfig(1.1), std::invalid_argument);
    EXPECT_THROW(AlignmentSearchConfig(0.2, 0.4), std::invalid_argument);
    EXPECT_THROW(AlignmentSearchConfig(0.2, 0.65, 1.0), std::invalid_argument);
    EXPECT_THROW(AlignmentSearchConfig(0.2, 0.65, 1.8, -0.1), std::invalid_argument);
    EXPECT_THROW(AlignmentSearchConfig(0.2, 0.65, 1.8, 0.6, 1.1), std::invalid_argument);
    EXPECT_THROW(AlignmentSearchConfig(0.2, 0.65, 1.8, 0.6, 0.05, 0), std::invalid_argument);
    EXPECT_THROW(NGramMatcher(1), std::invalid_argument);
}

TEST(AdaptiveWindowTests, FindsTheRepeatedPhraseNearestToTheRatioEstimate) {
    const auto book_words = TextNormalizer::tokenize(read_fixture("alignment/repeated-book.txt"));
    const auto spoken_words =
        TextNormalizer::tokenize(read_fixture("alignment/repeated-audio.txt"));
    const auto second_occurrence =
        static_cast<std::int32_t>(std::find_end(book_words.begin(), book_words.end(),
                                                spoken_words.begin(), spoken_words.end()) -
                                  book_words.begin());

    const auto match = AdaptiveWindowSearcher{}.search(book_words, spoken_words, second_occurrence,
                                                       AlignmentSearchConfig(0.4, 0.65, 1.8, 0.7));

    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->word_offset.value, second_occurrence);
    EXPECT_DOUBLE_EQ(match->confidence, 1.0);
}

TEST(AdaptiveWindowTests, ExpandsSearchWhenRatioEstimateMissesTheMatchingPassage) {
    std::vector<std::string> book_words;
    for (int index = 0; index < 90; ++index) {
        book_words.push_back("chapter" + std::to_string(index));
    }
    const auto spoken_words =
        TextNormalizer::tokenize("the lighthouse keeper watched the storm pass");
    book_words.insert(book_words.end(), spoken_words.begin(), spoken_words.end());
    for (int index = 0; index < 10; ++index) {
        book_words.push_back("ending" + std::to_string(index));
    }

    const auto match = AdaptiveWindowSearcher{}.search(book_words, spoken_words, 0,
                                                       AlignmentSearchConfig(0.1, 0.65, 1.8, 0.7));

    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->word_offset.value, 90);
}

TEST(AdaptiveWindowTests, AlignsAbridgedAudioDespiteMissingIntroAndOutro) {
    const auto book_words = TextNormalizer::tokenize(read_fixture("alignment/abridged-book.txt"));
    const auto spoken_words =
        TextNormalizer::tokenize(read_fixture("alignment/abridged-audio.txt"));
    const auto expected_offset = static_cast<std::int32_t>(
        std::find(book_words.begin(), book_words.end(), spoken_words.front()) - book_words.begin());
    const auto match =
        AdaptiveWindowSearcher{}.search(book_words, spoken_words, expected_offset,
                                        AlignmentSearchConfig(1.0, 0.65, 1.8, 0.55, 0.0));

    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->word_offset.value, expected_offset);
    EXPECT_GT(match->confidence, 0.7);
}

TEST(AdaptiveWindowTests, ToleratesNoisySpeechAndDifferentNarrationPacing) {
    const auto book_words =
        TextNormalizer::tokenize("the old traveler crossed the quiet bridge before dawn");
    const auto noisy_words = TextNormalizer::tokenize(read_fixture("alignment/noisy-audio.txt"));
    const auto match = AdaptiveWindowSearcher{}.search(
        book_words, noisy_words, 0, AlignmentSearchConfig(1.0, 0.65, 1.8, 0.4, 0.0));
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->word_offset.value, 1);

    const auto paced_book =
        TextNormalizer::tokenize("the narrator reads at a measured pace and reaches the doorway");
    const auto paced_transcript =
        TextNormalizer::tokenize("narrator reads measured pace reaches doorway");
    const auto paced_match = AdaptiveWindowSearcher{}.search(
        paced_book, paced_transcript, 0, AlignmentSearchConfig(1.0, 0.65, 1.8, 0.4, 0.0));
    ASSERT_TRUE(paced_match.has_value());
    EXPECT_EQ(paced_match->word_offset.value, 1);
}

TEST(AdaptiveWindowTests, RejectsWrongEditionAudioAndTooShortBooks) {
    const auto book_words =
        TextNormalizer::tokenize("the original edition contains entirely unrelated material");
    const auto wrong_edition =
        TextNormalizer::tokenize(read_fixture("alignment/wrong-edition-audio.txt"));
    EXPECT_FALSE(
        AdaptiveWindowSearcher{}
            .search(book_words, wrong_edition, 1, AlignmentSearchConfig(1.0, 0.65, 1.8, 0.8))
            .has_value());
    EXPECT_FALSE(AdaptiveWindowSearcher{}.search({"one", "two"}, {"one", "two"}, 0).has_value());
    EXPECT_FALSE(AdaptiveWindowSearcher{}.search({}, {"audio"}, 0).has_value());
}

TEST(AdaptiveWindowTests, HandlesVeryLongBooksAndUsesAsymmetricBoundedWindows) {
    std::vector<std::string> long_book;
    long_book.reserve(100'006);
    for (int index = 0; index < 50'000; ++index) {
        long_book.push_back("filler" + std::to_string(index));
    }
    const auto phrase = TextNormalizer::tokenize("the captain watched the distant horizon");
    long_book.insert(long_book.end(), phrase.begin(), phrase.end());
    for (int index = 0; index < 50'000; ++index) {
        long_book.push_back("ending" + std::to_string(index));
    }

    const auto match = AdaptiveWindowSearcher{}.search(long_book, phrase, 50'010,
                                                       AlignmentSearchConfig(0.01, 0.65, 1.8, 0.7));
    ASSERT_TRUE(match.has_value());
    EXPECT_EQ(match->word_offset.value, 50'000);

    const auto window = AdaptiveWindowSearcher{}.window_for(100, 50, 0.2, 0.7);
    EXPECT_EQ(window, (alignment::SearchWindow{36, 56}));
    EXPECT_EQ(AdaptiveWindowSearcher{}.window_for(100, 0, 0.2).start_word, 0);
    EXPECT_EQ(AdaptiveWindowSearcher{}.window_for(100, 100, 0.2).end_word_exclusive, 100);
    EXPECT_EQ(AdaptiveWindowSearcher{}.window_for(0, 0, 0.2), (alignment::SearchWindow{0, 0}));
    EXPECT_THROW(AdaptiveWindowSearcher{}.window_for(100, 0, 1.1), std::invalid_argument);
}

TEST(NGramMatcherTests, DoesNotSearchOutsideTheRequestedWindow) {
    const auto book_words = TextNormalizer::tokenize(
        "the traveler crossed the quiet bridge before dawn unrelated chapter content");
    const auto match = NGramMatcher{}.find_best_match(
        book_words, TextNormalizer::tokenize("the traveler crossed the quiet bridge before dawn"),
        8, book_words.size(), 0);
    EXPECT_FALSE(match.has_value());
    EXPECT_FALSE(
        NGramMatcher{}.find_best_match(book_words, {"tiny"}, 0, book_words.size(), 0).has_value());
}

} // namespace
} // namespace goodreader::tests
