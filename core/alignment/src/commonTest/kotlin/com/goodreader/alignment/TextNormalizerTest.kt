package com.goodreader.alignment

import kotlin.test.Test
import kotlin.test.assertEquals

class TextNormalizerTest {
    @Test
    fun foldsPunctuationCaseRussianYoAndDiacritics() {
        assertEquals(
            listOf("еще", "cafe", "1000", "слов"),
            TextNormalizer.tokenize("ЕЩЁ! Café, 1,000 слов"),
        )
    }
}
