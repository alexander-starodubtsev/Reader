/// A library record represented without exposing persistence types to SwiftUI.
public struct AppleLibraryEntry: Identifiable, Equatable, Codable {
    /// Stable local database identifier.
    public let id: String
    /// User-visible title.
    public let title: String
    /// Author when metadata is available.
    public let author: String?
    /// Imported book format.
    public let format: String
    /// App-owned local file URL.
    public let fileUri: String
    /// Number of paired audio chapters.
    public let audioTrackCount: Int
    /// Ordered app-owned audio files available to the built-in player.
    public let audioTracks: [AppleLibraryAudioTrack]

    /// Creates a library entry for the native application layer.
    public init(
        id: String,
        title: String,
        author: String?,
        format: String,
        fileUri: String,
        audioTrackCount: Int,
        audioTracks: [AppleLibraryAudioTrack] = []
    ) {
        self.id = id
        self.title = title
        self.author = author
        self.format = format
        self.fileUri = fileUri
        self.audioTrackCount = audioTrackCount
        self.audioTracks = audioTracks
    }

    private enum CodingKeys: String, CodingKey {
        case id
        case title
        case author
        case format
        case fileUri
        case audioTrackCount
        case audioTracks
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        id = try container.decode(String.self, forKey: .id)
        title = try container.decode(String.self, forKey: .title)
        author = try container.decodeIfPresent(String.self, forKey: .author)
        format = try container.decode(String.self, forKey: .format)
        fileUri = try container.decode(String.self, forKey: .fileUri)
        audioTracks = try container.decodeIfPresent([AppleLibraryAudioTrack].self, forKey: .audioTracks) ?? []
        audioTrackCount = try container.decodeIfPresent(Int.self, forKey: .audioTrackCount) ?? audioTracks.count
    }
}

/// A persisted chapter in the audiobook paired with a library entry.
public struct AppleLibraryAudioTrack: Identifiable, Equatable, Codable {
    public let id: String
    public let uri: String
    public let durationMillis: Int64
    public let chapterIndex: Int

    public init(id: String, uri: String, durationMillis: Int64, chapterIndex: Int) {
        self.id = id
        self.uri = uri
        self.durationMillis = durationMillis
        self.chapterIndex = chapterIndex
    }
}

/// Audio metadata provided by the native file importer.
public struct AppleAudioInput {
    /// App-owned file URL.
    public let uri: String
    /// Supported lowercase file extension.
    public let fileExtension: String
    /// Media duration in milliseconds.
    public let durationMillis: Int64
    /// Zero-based chapter order.
    public let chapterIndex: Int

    /// Creates an audio input for a selected recording.
    public init(
        uri: String,
        fileExtension: String,
        durationMillis: Int64,
        chapterIndex: Int
    ) {
        self.uri = uri
        self.fileExtension = fileExtension
        self.durationMillis = durationMillis
        self.chapterIndex = chapterIndex
    }
}

/// Persistence boundary used by the Apple library view model.
public protocol AppleLibraryStore {
    /// Searches persisted books.
    func searchBooks(_ query: String?) async throws -> [AppleLibraryEntry]

    /// Persists a user-selected book and its optional audio pairing.
    func importBook(
        fileUri: String,
        title: String,
        fileExtension: String,
        audioFiles: [AppleAudioInput]
    ) async throws -> AppleLibraryEntry
}
