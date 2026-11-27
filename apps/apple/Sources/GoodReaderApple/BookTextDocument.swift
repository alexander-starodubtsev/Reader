import Foundation
import PDFKit
import GoodReaderZip

struct BookTextDocument: Sendable {
    let title: String
    let author: String?
    let paragraphs: [String]

    static func open(_ url: URL) throws -> BookTextDocument {
        switch url.pathExtension.lowercased() {
        case "pdf":
            try openPDF(url)
        case "fb2":
            try openFB2(url)
        case "epub":
            try openEPUB(url)
        default:
            throw ReaderError.unsupportedFormat(url.pathExtension)
        }
    }

    private static func openPDF(_ url: URL) throws -> BookTextDocument {
        guard let pdf = PDFDocument(url: url) else {
            throw ReaderError.corruptBook("PDFKit could not open this PDF.")
        }
        let paragraphs = (0..<pdf.pageCount).compactMap { pdf.page(at: $0)?.string }
            .flatMap { $0.components(separatedBy: .newlines) }
            .map(\.trimmed)
            .filter { !$0.isEmpty }
        guard !paragraphs.isEmpty else {
            throw ReaderError.noReadableText("This PDF has no selectable text. Scanned PDFs are not supported yet.")
        }
        return BookTextDocument(title: url.deletingPathExtension().lastPathComponent,
                                author: nil, paragraphs: paragraphs)
    }

    private static func openFB2(_ url: URL) throws -> BookTextDocument {
        let data = try Data(contentsOf: url)
        let parser = FictionBookParser(data: data)
        guard parser.parse() else {
            throw ReaderError.corruptBook(parser.parserError?.localizedDescription ?? "The FB2 XML is invalid.")
        }
        guard !parser.paragraphs.isEmpty else {
            throw ReaderError.noReadableText("This FB2 file contains no readable paragraphs.")
        }
        return BookTextDocument(
            title: parser.title ?? url.deletingPathExtension().lastPathComponent,
            author: parser.author,
            paragraphs: parser.paragraphs
        )
    }

    private static func openEPUB(_ url: URL) throws -> BookTextDocument {
        let archive = try EPUBArchive(data: Data(contentsOf: url))
        let container = try archive.member("META-INF/container.xml")
        guard let containerText = String(data: container, encoding: .utf8),
              let packagePath = containerText.firstCapture(of: #"full-path\s*=\s*["']([^"']+)["']"#) else {
            throw ReaderError.corruptBook("The EPUB container file is missing its package document.")
        }

        let packageData = try archive.member(packagePath)
        let package = EPUBPackageParser(data: packageData)
        guard package.parse(), !package.spine.isEmpty else {
            throw ReaderError.corruptBook(package.parserError?.localizedDescription ?? "The EPUB reading order is invalid.")
        }

        var paragraphs: [String] = []
        let basePath = (packagePath as NSString).deletingLastPathComponent
        for id in package.spine {
            guard let href = package.manifest[id] else { continue }
            let memberPath = URL(fileURLWithPath: "/\(basePath)/\(href)")
                .standardizedFileURL.path.dropFirst()
            let parser = XHTMLTextParser(data: try archive.member(String(memberPath)))
            guard parser.parse() else {
                throw ReaderError.corruptBook(parser.parserError?.localizedDescription ?? "An EPUB chapter is invalid.")
            }
            paragraphs.append(contentsOf: parser.paragraphs)
        }
        guard !paragraphs.isEmpty else {
            throw ReaderError.noReadableText("This EPUB contains no readable paragraphs.")
        }
        return BookTextDocument(
            title: package.title ?? url.deletingPathExtension().lastPathComponent,
            author: package.author,
            paragraphs: paragraphs
        )
    }
}

enum ReaderError: LocalizedError {
    case unsupportedFormat(String)
    case corruptBook(String)
    case noReadableText(String)

    var errorDescription: String? {
        switch self {
        case .unsupportedFormat(let format): "The \(format) format is not supported."
        case .corruptBook(let reason), .noReadableText(let reason): reason
        }
    }
}

private struct EPUBArchive {
    private let bytes: [UInt8]

    init(data: Data) throws {
        guard data.count <= 128 * 1024 * 1024 else {
            throw ReaderError.corruptBook("The EPUB file is larger than the supported limit.")
        }
        bytes = Array(data)
    }

    func member(_ path: String) throws -> Data {
        let lowerBound = max(0, bytes.count - 65_557)
        guard let endRecord = (lowerBound..<max(lowerBound, bytes.count - 3))
            .reversed().first(where: { read32($0) == 0x06054b50 }) else {
            throw ReaderError.corruptBook("The EPUB archive directory is missing.")
        }
        let entryCount = Int(read16(endRecord + 10))
        var cursor = Int(read32(endRecord + 16))

        for _ in 0..<entryCount {
            guard read32(cursor) == 0x02014b50 else {
                throw ReaderError.corruptBook("The EPUB archive directory is invalid.")
            }
            let compressedSize = Int(read32(cursor + 20))
            let expandedSize = Int(read32(cursor + 24))
            let nameLength = Int(read16(cursor + 28))
            let extraLength = Int(read16(cursor + 30))
            let commentLength = Int(read16(cursor + 32))
            let localHeader = Int(read32(cursor + 42))
            let nameStart = cursor + 46
            guard nameStart + nameLength <= bytes.count,
                  let name = String(bytes: bytes[nameStart..<(nameStart + nameLength)], encoding: .utf8) else {
                throw ReaderError.corruptBook("The EPUB contains an invalid archive path.")
            }
            if name == path {
                let method = read16(cursor + 10)
                guard read32(localHeader) == 0x04034b50 else {
                    throw ReaderError.corruptBook("The EPUB chapter header is invalid.")
                }
                let localNameLength = Int(read16(localHeader + 26))
                let localExtraLength = Int(read16(localHeader + 28))
                let dataStart = localHeader + 30 + localNameLength + localExtraLength
                guard dataStart + compressedSize <= bytes.count,
                      expandedSize <= 64 * 1024 * 1024 else {
                    throw ReaderError.corruptBook("The EPUB chapter is truncated or too large.")
                }
                let compressed = Data(bytes[dataStart..<(dataStart + compressedSize)])
                if method == 0 { return compressed }
                guard method == 8 else {
                    throw ReaderError.corruptBook("The EPUB uses an unsupported compression method.")
                }
                var expanded = Data(count: expandedSize)
                let status = compressed.withUnsafeBytes { input in
                    expanded.withUnsafeMutableBytes { output in
                        goodreader_inflate_raw(
                            input.bindMemory(to: UInt8.self).baseAddress,
                            compressedSize,
                            output.bindMemory(to: UInt8.self).baseAddress,
                            expandedSize
                        )
                    }
                }
                guard status == 0 else {
                    throw ReaderError.corruptBook("The EPUB chapter could not be decompressed.")
                }
                return expanded
            }
            cursor = nameStart + nameLength + extraLength + commentLength
        }
        throw ReaderError.corruptBook("The EPUB is missing \(path).")
    }

    private func read16(_ offset: Int) -> UInt16 {
        guard offset >= 0, offset + 2 <= bytes.count else { return 0 }
        return UInt16(bytes[offset]) | UInt16(bytes[offset + 1]) << 8
    }

    private func read32(_ offset: Int) -> UInt32 {
        guard offset >= 0, offset + 4 <= bytes.count else { return 0 }
        return UInt32(bytes[offset]) | UInt32(bytes[offset + 1]) << 8
            | UInt32(bytes[offset + 2]) << 16 | UInt32(bytes[offset + 3]) << 24
    }
}

private final class EPUBPackageParser: NSObject, XMLParserDelegate {
    private let parser: XMLParser
    private var currentTextElement: String?
    private var text = ""
    private var currentManifestID: String?
    var manifest: [String: String] = [:]
    var spine: [String] = []
    var title: String?
    var author: String?

    init(data: Data) {
        parser = XMLParser(data: data)
    }

    func parse() -> Bool {
        parser.delegate = self
        return parser.parse()
    }

    var parserError: Error? { parser.parserError }

    func parser(_ parser: XMLParser, didStartElement elementName: String, namespaceURI: String?,
                qualifiedName qName: String?, attributes: [String: String] = [:]) {
        switch elementName.lowercased() {
        case "item":
            if let id = attributes["id"], let href = attributes["href"] {
                manifest[id] = href
            }
        case "itemref":
            if let id = attributes["idref"] { spine.append(id) }
        case "title", "creator":
            currentTextElement = elementName.lowercased()
            text = ""
        default:
            break
        }
    }

    func parser(_ parser: XMLParser, foundCharacters string: String) {
        if currentTextElement != nil { text += string }
    }

    func parser(_ parser: XMLParser, didEndElement elementName: String, namespaceURI: String?,
                qualifiedName qName: String?) {
        guard currentTextElement == elementName.lowercased() else { return }
        let value = text.trimmed
        if currentTextElement == "title" { title = value }
        if currentTextElement == "creator" { author = value }
        currentTextElement = nil
        text = ""
    }
}

private final class XHTMLTextParser: NSObject, XMLParserDelegate {
    private let parser: XMLParser
    private var activeElement: String?
    private var text = ""
    var paragraphs: [String] = []

    init(data: Data) {
        parser = XMLParser(data: data)
    }

    func parse() -> Bool {
        parser.delegate = self
        return parser.parse()
    }

    var parserError: Error? { parser.parserError }

    func parser(_ parser: XMLParser, didStartElement elementName: String, namespaceURI: String?,
                qualifiedName qName: String?, attributes: [String: String] = [:]) {
        let name = elementName.lowercased()
        if activeElement == nil, ["p", "h1", "h2", "h3", "li", "blockquote"].contains(name) {
            activeElement = name
            text = ""
        }
    }

    func parser(_ parser: XMLParser, foundCharacters string: String) {
        if activeElement != nil { text += string }
    }

    func parser(_ parser: XMLParser, didEndElement elementName: String, namespaceURI: String?,
                qualifiedName qName: String?) {
        guard activeElement == elementName.lowercased() else { return }
        let value = text.trimmed
        if !value.isEmpty { paragraphs.append(value) }
        activeElement = nil
        text = ""
    }
}

private final class FictionBookParser: NSObject, XMLParserDelegate {
    private let parser: XMLParser
    private var activeElement: String?
    private var text = ""
    private var firstName: String?
    private var lastName: String?
    var title: String?
    var author: String?
    var paragraphs: [String] = []

    init(data: Data) {
        parser = XMLParser(data: data)
    }

    func parse() -> Bool {
        parser.delegate = self
        return parser.parse()
    }

    var parserError: Error? { parser.parserError }

    func parser(_ parser: XMLParser, didStartElement elementName: String, namespaceURI: String?,
                qualifiedName qName: String?, attributes: [String: String] = [:]) {
        let name = elementName.lowercased()
        if activeElement == nil,
           ["book-title", "first-name", "last-name", "p"].contains(name) {
            activeElement = name
            text = ""
        }
    }

    func parser(_ parser: XMLParser, foundCharacters string: String) {
        if activeElement != nil { text += string }
    }

    func parser(_ parser: XMLParser, didEndElement elementName: String, namespaceURI: String?,
                qualifiedName qName: String?) {
        guard activeElement == elementName.lowercased() else { return }
        let value = text.trimmed
        switch activeElement {
        case "book-title": title = value
        case "first-name": firstName = value
        case "last-name":
            lastName = value
            author = [firstName, lastName].compactMap { $0 }.joined(separator: " ")
        case "p": if !value.isEmpty { paragraphs.append(value) }
        default: break
        }
        activeElement = nil
        text = ""
    }
}

private extension String {
    var trimmed: String { trimmingCharacters(in: .whitespacesAndNewlines) }

    func firstCapture(of pattern: String) -> String? {
        guard let range = range(of: pattern, options: .regularExpression),
              let capture = self[range].range(of: #"["']([^"']+)["']"#, options: .regularExpression) else {
            return nil
        }
        return String(self[capture].dropFirst().dropLast())
    }
}
