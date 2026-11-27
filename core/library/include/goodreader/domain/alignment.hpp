#pragma once

#include "goodreader/domain/types.hpp"

#include <optional>
#include <vector>

namespace goodreader::domain {

/** A word position and confidence score identified by a text search. */
struct AlignmentMatch {
    WordOffset word_offset;
    double confidence;
    double margin;

    /** Creates a location with confidence and margin in the inclusive range [0, 1]. */
    AlignmentMatch(WordOffset word_offset, double confidence, double margin);
    bool operator==(const AlignmentMatch&) const = default;
};

} // namespace goodreader::domain
