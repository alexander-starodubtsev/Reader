import AVFoundation
import Foundation

actor AppleSelectedFileStore {
    func stage(_ source: URL) async throws -> URL {
        try await Task.detached(priority: .userInitiated) {
            let hasScopedAccess = source.startAccessingSecurityScopedResource()
            defer {
                if hasScopedAccess {
                    source.stopAccessingSecurityScopedResource()
                }
            }
            let fileManager = FileManager.default
            let appSupport = try fileManager.url(
                for: .applicationSupportDirectory,
                in: .userDomainMask,
                appropriateFor: nil,
                create: true
            )
            let directory = appSupport.appendingPathComponent("GoodReader/Books", isDirectory: true)
            try fileManager.createDirectory(at: directory, withIntermediateDirectories: true)
            let destination = directory
                .appendingPathComponent(UUID().uuidString)
                .appendingPathExtension(source.pathExtension.lowercased())
            try fileManager.copyItem(at: source, to: destination)
            return destination
        }.value
    }

    func remove(_ urls: [URL]) async throws {
        try await Task.detached(priority: .utility) {
            for url in urls where FileManager.default.fileExists(atPath: url.path) {
                try FileManager.default.removeItem(at: url)
            }
        }.value
    }

    static func durationMillis(for url: URL) async throws -> Int64 {
        if url.pathExtension.lowercased() == "ogg" {
            return try oggDurationMillis(url)
        }
        let asset = AVURLAsset(url: url)
        let duration = try await asset.load(.duration)
        let seconds = duration.seconds
        guard seconds.isFinite, seconds > 0 else {
            throw AppleImportError.unreadableAudio
        }
        return Int64((seconds * 1_000).rounded())
    }

    private static func oggDurationMillis(_ url: URL) throws -> Int64 {
        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }

        let header = try handle.read(upToCount: 64 * 1024) ?? Data()
        let opusMarker = Data("OpusHead".utf8)
        let vorbisMarker = Data("vorbis".utf8)
        let sampleRate: UInt32
        let preSkip: UInt64
        if let marker = header.range(of: opusMarker) {
            let start = header.distance(from: header.startIndex, to: marker.lowerBound)
            guard let skip = uint16LE(header, at: start + 10) else {
                throw AppleImportError.unreadableAudio
            }
            sampleRate = 48_000
            preSkip = UInt64(skip)
        } else if let marker = header.range(of: vorbisMarker) {
            let start = header.distance(from: header.startIndex, to: marker.lowerBound)
            guard let rate = uint32LE(header, at: start + 11), rate > 0 else {
                throw AppleImportError.unreadableAudio
            }
            sampleRate = rate
            preSkip = 0
        } else {
            throw AppleImportError.unsupportedOgg
        }

        let fileSize = try handle.seekToEnd()
        let tailSize = min(fileSize, 128 * 1024)
        try handle.seek(toOffset: fileSize - tailSize)
        let tail = try handle.read(upToCount: Int(tailSize)) ?? Data()
        guard tail.count >= 14 else { throw AppleImportError.unreadableAudio }

        var finalGranule: UInt64 = 0
        for offset in 0...(tail.count - 14) {
            guard tail[offset] == 0x4F, tail[offset + 1] == 0x67,
                  tail[offset + 2] == 0x67, tail[offset + 3] == 0x53,
                  tail[offset + 4] == 0
            else {
                continue
            }
            guard let granule = uint64LE(tail, at: offset + 6), granule != UInt64.max else { continue }
            let segmentCount = Int(tail[offset + 26])
            guard offset + 27 + segmentCount <= tail.count else { continue }
            let bodyLength = (0..<segmentCount).reduce(0) { $0 + Int(tail[offset + 27 + $1]) }
            guard offset + 27 + segmentCount + bodyLength <= tail.count else { continue }
            finalGranule = max(finalGranule, granule)
        }
        guard finalGranule > preSkip else { throw AppleImportError.unreadableAudio }
        let samples = finalGranule - preSkip
        return Int64((Double(samples) / Double(sampleRate) * 1_000).rounded())
    }

    private static func uint16LE(_ data: Data, at offset: Int) -> UInt16? {
        guard offset >= 0, offset + 2 <= data.count else { return nil }
        return UInt16(data[offset]) | (UInt16(data[offset + 1]) << 8)
    }

    private static func uint32LE(_ data: Data, at offset: Int) -> UInt32? {
        guard offset >= 0, offset + 4 <= data.count else { return nil }
        return UInt32(data[offset]) |
            (UInt32(data[offset + 1]) << 8) |
            (UInt32(data[offset + 2]) << 16) |
            (UInt32(data[offset + 3]) << 24)
    }

    private static func uint64LE(_ data: Data, at offset: Int) -> UInt64? {
        guard offset >= 0, offset + 8 <= data.count else { return nil }
        return (0..<8).reduce(UInt64(0)) { value, byte in
            value | (UInt64(data[offset + byte]) << (byte * 8))
        }
    }
}

enum AppleImportError: LocalizedError {
    case unreadableAudio
    case unsupportedOgg

    var errorDescription: String? {
        switch self {
        case .unreadableAudio:
            "The selected audio file has no readable duration."
        case .unsupportedOgg:
            "The selected OGG file is not Vorbis or Opus audio."
        }
    }
}
