package com.goodreader.windows

import java.nio.file.Files
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith

class WindowsBookReaderTest {
    @Test
    fun extractsTextAndMetadataFromFb2() {
        val file = Files.createTempFile("goodreader-reader", ".fb2")
        try {
            Files.writeString(
                file,
                """
                <FictionBook>
                  <description><title-info>
                    <book-title>Sample Story</book-title>
                    <author><first-name>Alex</first-name><last-name>Writer</last-name></author>
                  </title-info></description>
                  <body><section><p>First paragraph.</p><p>Second paragraph.</p></section></body>
                </FictionBook>
                """.trimIndent(),
            )

            val document = WindowsBookTextDocument.open(file.toFile())

            assertEquals("Sample Story", document.title)
            assertEquals("Alex Writer", document.author)
            assertEquals(listOf("First paragraph.", "Second paragraph."), document.paragraphs)
        } finally {
            Files.deleteIfExists(file)
        }
    }

    @Test
    fun followsEpubSpineAndExtractsChapterText() {
        val file = Files.createTempFile("goodreader-reader", ".epub")
        try {
            ZipOutputStream(Files.newOutputStream(file)).use { zip ->
                zip.addTextEntry(
                    "META-INF/container.xml",
                    """<container><rootfiles><rootfile full-path="OPS/package.opf"/></rootfiles></container>""",
                )
                zip.addTextEntry(
                    "OPS/package.opf",
                    """
                    <package>
                      <metadata><title>EPUB Story</title><creator>Sam Author</creator></metadata>
                      <manifest><item id="chapter" href="chapter.xhtml"/></manifest>
                      <spine><itemref idref="chapter"/></spine>
                    </package>
                    """.trimIndent(),
                )
                zip.addTextEntry(
                    "OPS/chapter.xhtml",
                    """<html><body><h1>Chapter One</h1><p>Readable EPUB text.</p></body></html>""",
                )
            }

            val document = WindowsBookTextDocument.open(file.toFile())

            assertEquals("EPUB Story", document.title)
            assertEquals("Sam Author", document.author)
            assertEquals(listOf("Chapter One", "Readable EPUB text."), document.paragraphs)
        } finally {
            Files.deleteIfExists(file)
        }
    }

    @Test
    fun rejectsBooksWithoutReadableText() {
        val file = Files.createTempFile("goodreader-reader", ".fb2")
        try {
            Files.writeString(file, "<FictionBook><body/></FictionBook>")

            assertFailsWith<IllegalArgumentException> {
                WindowsBookTextDocument.open(file.toFile())
            }
        } finally {
            Files.deleteIfExists(file)
        }
    }

    private fun ZipOutputStream.addTextEntry(
        path: String,
        text: String,
    ) {
        putNextEntry(ZipEntry(path))
        write(text.toByteArray())
        closeEntry()
    }
}
