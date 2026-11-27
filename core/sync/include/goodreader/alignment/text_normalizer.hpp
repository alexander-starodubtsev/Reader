#pragma once

#include <string>
#include <vector>

namespace goodreader::alignment {

/** Converts Unicode text into comparable lowercase alphanumeric tokens. */
class TextNormalizer {
  public:
    /** Folds case and diacritics, folds ё to е, and discards punctuation. */
    static std::vector<std::string> tokenize(const std::string& text);
};

} // namespace goodreader::alignment
