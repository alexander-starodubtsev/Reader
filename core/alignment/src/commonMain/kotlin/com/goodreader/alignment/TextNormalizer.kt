package com.goodreader.alignment

/** Normalizes text into comparable lowercase tokens for audio-to-text alignment. */
object TextNormalizer {
    /** Removes punctuation and diacritics, folds ё to е, and keeps alphanumeric tokens. */
    fun tokenize(text: String): List<String> {
        val tokens = mutableListOf<String>()
        val current = StringBuilder()
        text.lowercase().forEachIndexed { index, original ->
            val character = fold(original)
            when {
                character in '\u0300'..'\u036f' -> Unit
                character.isLetterOrDigit() -> current.append(character)
                isNumericSeparator(text, index) -> Unit
                else -> flush(current, tokens)
            }
        }
        flush(current, tokens)
        return tokens
    }

    private fun flush(
        current: StringBuilder,
        tokens: MutableList<String>,
    ) {
        if (current.isNotEmpty()) {
            tokens += current.toString()
            current.clear()
        }
    }

    private fun isNumericSeparator(
        text: String,
        index: Int,
    ): Boolean {
        val character = text[index]
        return (character == ',' || character == '.') &&
            index > 0 &&
            index < text.lastIndex &&
            text[index - 1].isDigit() &&
            text[index + 1].isDigit()
    }

    private fun fold(character: Char): Char =
        when (character) {
            'ё' -> 'е'
            'á', 'à', 'â', 'ä', 'ã', 'å', 'ā' -> 'a'
            'é', 'è', 'ê', 'ë', 'ē' -> 'e'
            'í', 'ì', 'î', 'ï', 'ī' -> 'i'
            'ó', 'ò', 'ô', 'ö', 'õ', 'ō' -> 'o'
            'ú', 'ù', 'û', 'ü', 'ū' -> 'u'
            'ý', 'ÿ' -> 'y'
            'ñ' -> 'n'
            'ç' -> 'c'
            else -> character
        }
}
