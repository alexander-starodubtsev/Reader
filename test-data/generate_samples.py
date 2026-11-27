#!/usr/bin/env python3
"""Generate original GoodReader book fixtures and matching macOS TTS audio."""

from __future__ import annotations

import html
import shutil
import subprocess
import tempfile
import wave
import zipfile
from pathlib import Path
from xml.etree import ElementTree


OUTPUT = Path(__file__).resolve().parent
CHAPTERS = [
    (
        "The Lantern",
        """Mara found the brass lantern beneath a loose board in the old ferry house.
        It was small enough to hold in one hand, but its glass was clear and bright.
        A note tied to the handle said, Take this to the north pier before the fog
        comes in. No name followed, and no one in the village admitted leaving it.
        Mara lit the wick and watched a calm gold circle spread across the dusty floor.
        Outside, gulls turned above the harbor, and the tide was already beginning to rise.
        She remembered her grandmother saying that every harbor light was a promise:
        someone was watching, and someone would find the shore. Mara tucked the note
        into her coat and closed the ferry house door behind her.""",
    ),
    (
        "The Crossing",
        """She carried the lantern along the seawall as evening settled over the roofs.
        At the north pier, a little wooden boat bumped softly against its rope.
        An old man stood beside it, holding a map that the damp air had curled at
        the edges. He looked relieved when he saw the light. My sister is waiting
        across the inlet, he explained. The fog hides the channel markers, and I
        cannot leave without a guide. Mara set the lantern in the bow, where its
        reflection made a narrow road across the dark water. The man offered her
        the map, but she knew these waters from summers spent rowing with her father.
        She pointed toward the quiet gap between two black rocks and told him to keep
        the lantern in sight. Then she pushed the boat gently away from the pier.""",
    ),
    (
        "A Safe Return",
        """The boat moved slowly, following the lantern's trembling path. From the
        shore, Mara could just make out a second light answering from the far bank.
        The old man raised one hand, and the boat turned toward it. When it reached
        the landing, a woman stepped forward and laughed with joy. They had been
        apart for many years, but they recognized each other at once. Mara waited
        until both were safely ashore, then carried the empty lantern home. At dawn
        she found a fresh note beneath her door: The light is yours whenever someone
        needs help finding the way. This time, it bore no signature either.
        Mara cleaned the lantern and placed it by the window, ready for another night.""",
    ),
]


def all_chapter_text() -> str:
    return "\n\n".join(f"{title}\n{text.strip()}" for title, text in CHAPTERS)


def write_epub(path: Path) -> None:
    package = ElementTree.Element(
        "package",
        {
            "xmlns": "http://www.idpf.org/2007/opf",
            "version": "3.0",
            "unique-identifier": "book-id",
            "xml:lang": "en",
        },
    )
    metadata = ElementTree.SubElement(package, "metadata", {"xmlns:dc": "http://purl.org/dc/elements/1.1/"})
    ElementTree.SubElement(metadata, "{http://purl.org/dc/elements/1.1/}identifier", {"id": "book-id"}).text = "urn:uuid:goodreader-sample-book"
    ElementTree.SubElement(metadata, "{http://purl.org/dc/elements/1.1/}title").text = "The Lantern at North Pier"
    ElementTree.SubElement(metadata, "{http://purl.org/dc/elements/1.1/}language").text = "en"
    manifest = ElementTree.SubElement(package, "manifest")
    ElementTree.SubElement(manifest, "item", {"id": "chapter", "href": "chapter.xhtml", "media-type": "application/xhtml+xml"})
    ElementTree.SubElement(manifest, "item", {"id": "nav", "href": "nav.xhtml", "media-type": "application/xhtml+xml", "properties": "nav"})
    spine = ElementTree.SubElement(package, "spine")
    ElementTree.SubElement(spine, "itemref", {"idref": "chapter"})

    xhtml_parts = [
        '<?xml version="1.0" encoding="utf-8"?>',
        '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" lang="en">',
        "<head><title>The Lantern at North Pier</title><meta charset=\"utf-8\" /></head><body>",
        "<h1>The Lantern at North Pier</h1>",
    ]
    for title, text in CHAPTERS:
        xhtml_parts.append(f"<section epub:type=\"chapter\"><h2>{html.escape(title)}</h2><p>{html.escape(text.strip())}</p></section>")
    xhtml_parts.append("</body></html>")
    chapter_xhtml = "\n".join(xhtml_parts)
    nav_xhtml = (
        '<?xml version="1.0" encoding="utf-8"?>'
        '<html xmlns="http://www.w3.org/1999/xhtml" '
        'xmlns:epub="http://www.idpf.org/2007/ops" lang="en"><head>'
        "<title>Contents</title></head><body><nav epub:type=\"toc\"><h1>Contents</h1>"
        "<ol><li><a href=\"chapter.xhtml\">The Lantern at North Pier</a></li></ol>"
        "</nav></body></html>"
    )

    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        archive.writestr(
            "META-INF/container.xml",
            '<?xml version="1.0" encoding="UTF-8"?>'
            '<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">'
            '<rootfiles><rootfile full-path="OEBPS/content.opf" '
            'media-type="application/oebps-package+xml"/></rootfiles></container>',
        )
        archive.writestr("OEBPS/content.opf", ElementTree.tostring(package, encoding="unicode"))
        archive.writestr("OEBPS/chapter.xhtml", chapter_xhtml)
        archive.writestr("OEBPS/nav.xhtml", nav_xhtml)


def write_fb2(path: Path) -> None:
    root = ElementTree.Element("FictionBook", {"xmlns": "http://www.gribuser.ru/xml/fictionbook/2.0"})
    description = ElementTree.SubElement(root, "description")
    title_info = ElementTree.SubElement(description, "title-info")
    ElementTree.SubElement(title_info, "genre").text = "prose"
    ElementTree.SubElement(title_info, "book-title").text = "The Lantern at North Pier"
    ElementTree.SubElement(title_info, "lang").text = "en"
    body = ElementTree.SubElement(root, "body")
    for title, text in CHAPTERS:
        section = ElementTree.SubElement(body, "section")
        ElementTree.SubElement(ElementTree.SubElement(section, "title"), "p").text = title
        ElementTree.SubElement(section, "p").text = text.strip()
    ElementTree.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)


def pdf_escape(text: str) -> bytes:
    value = text.encode("latin-1", errors="replace")
    return value.replace(b"\\", b"\\\\").replace(b"(", b"\\(").replace(b")", b"\\)")


def write_pdf(path: Path) -> None:
    import textwrap

    lines = ["The Lantern at North Pier", ""]
    for title, text in CHAPTERS:
        lines.extend([title, *textwrap.wrap(text.strip(), width=84), ""])
    commands = []
    y = 742
    for index, line in enumerate(lines):
        size = 16 if index == 0 else (12 if line in {title for title, _ in CHAPTERS} else 10)
        commands.append(b"BT /F1 " + str(size).encode() + b" Tf 54 " + str(y).encode() + b" Td (" + pdf_escape(line) + b") Tj ET")
        y -= 18 if size == 16 else 14
    stream = b"\n".join(commands)
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        b"<< /Length " + str(len(stream)).encode() + b" >>\nstream\n" + stream + b"\nendstream",
    ]
    pdf = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for number, body in enumerate(objects, start=1):
        offsets.append(len(pdf))
        pdf.extend(f"{number} 0 obj\n".encode())
        pdf.extend(body)
        pdf.extend(b"\nendobj\n")
    xref_offset = len(pdf)
    pdf.extend(f"xref\n0 {len(objects) + 1}\n".encode())
    pdf.extend(b"0000000000 65535 f \n")
    for offset in offsets[1:]:
        pdf.extend(f"{offset:010d} 00000 n \n".encode())
    pdf.extend(
        f"trailer\n<< /Size {len(objects) + 1} /Root 1 0 R >>\n"
        f"startxref\n{xref_offset}\n%%EOF\n".encode()
    )
    path.write_bytes(pdf)


def convert_aiff_to_wav(source: Path, destination: Path) -> None:
    subprocess.run(
        ["afconvert", "-f", "WAVE", "-d", "LEI16", str(source), str(destination)],
        check=True,
    )


def concatenate_wavs(sources: list[Path], destination: Path) -> None:
    with wave.open(str(sources[0]), "rb") as first:
        params = first.getparams()
        frames = [first.readframes(first.getnframes())]
    for source in sources[1:]:
        with wave.open(str(source), "rb") as item:
            item_params = item.getparams()
            if (
                item_params.nchannels != params.nchannels
                or item_params.sampwidth != params.sampwidth
                or item_params.framerate != params.framerate
                or item_params.comptype != params.comptype
            ):
                raise ValueError(f"Incompatible audio parameters in {source}")
            frames.append(item.readframes(item.getnframes()))
    with wave.open(str(destination), "wb") as output:
        output.setparams(params)
        for data in frames:
            output.writeframes(data)


def generate_audio() -> None:
    if not shutil.which("say") or not shutil.which("afconvert"):
        raise RuntimeError("macOS `say` and `afconvert` commands are required for sample audio.")
    with tempfile.TemporaryDirectory(prefix="goodreader-samples-") as temporary:
        temp = Path(temporary)
        wavs: list[Path] = []
        for index, (_, text) in enumerate(CHAPTERS, start=1):
            aiff = temp / f"chapter-{index}.aiff"
            wav = OUTPUT / f"sample_ch{index}.wav"
            subprocess.run(["say", "-o", str(aiff), text.strip()], check=True)
            convert_aiff_to_wav(aiff, wav)
            wavs.append(wav)
        concatenate_wavs(wavs, OUTPUT / "sample.wav")
        concatenate_wavs(wavs[1:], OUTPUT / "sample_missing_intro.wav")


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / "sample.txt").write_text(all_chapter_text() + "\n", encoding="utf-8")
    write_epub(OUTPUT / "sample.epub")
    write_fb2(OUTPUT / "sample.fb2")
    write_pdf(OUTPUT / "sample.pdf")
    generate_audio()
    word_count = sum(len(text.split()) for _, text in CHAPTERS)
    print(f"Generated original book with {word_count} words in {len(CHAPTERS)} chapters.")


if __name__ == "__main__":
    main()
