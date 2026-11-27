package com.goodreader.application

import kotlinx.coroutines.test.runTest as kotlinxRunTest

internal fun runTest(block: suspend () -> Unit) {
    kotlinxRunTest { block() }
}
