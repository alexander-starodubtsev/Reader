import Foundation
import GoodReaderApple
import GoodReaderCore

final class AppleKotlinLibraryStore: AppleLibraryStore {
    private let facade = AppleLibraryFacade()

    func searchBooks(_ query: String?) async throws -> [AppleLibraryEntry] {
        let values: [AppleLibraryBook] = try await invokeKotlin { completion in
            facade.searchBooks(query: query, completionHandler: completion)
        }
        return values.map(Self.entry(from:))
    }

    func importBook(
        fileUri: String,
        title: String,
        fileExtension: String,
        audioFiles: [AppleAudioInput]
    ) async throws -> AppleLibraryEntry {
        let files = audioFiles.map {
            AppleAudioFile(
                uri: $0.uri,
                fileExtension: $0.fileExtension,
                durationMillis: $0.durationMillis,
                chapterIndex: Int32($0.chapterIndex)
            )
        }
        let value: AppleLibraryBook = try await invokeKotlin { completion in
            facade.importBook(
                bookUri: fileUri,
                title: title,
                bookExtension: fileExtension,
                audioFiles: files,
                completionHandler: completion
            )
        }
        return Self.entry(from: value)
    }

    private static func entry(from value: AppleLibraryBook) -> AppleLibraryEntry {
        AppleLibraryEntry(
            id: value.id,
            title: value.title,
            author: value.author,
            format: value.format,
            fileUri: value.fileUri,
            audioTrackCount: value.audioTracks.count,
            audioTracks: value.audioTracks.map {
                AppleLibraryAudioTrack(
                    id: $0.id,
                    uri: $0.uri,
                    durationMillis: $0.durationMillis,
                    chapterIndex: Int($0.chapterIndex)
                )
            }
        )
    }
}

private func invokeKotlin<T>(
    _ operation: (@escaping (T?, Error?) -> Void) -> Void
) async throws -> T {
    try await withCheckedThrowingContinuation { continuation in
        operation { value, error in
            if let error {
                continuation.resume(throwing: error)
            } else if let value {
                continuation.resume(returning: value)
            } else {
                continuation.resume(throwing: AppleLibraryBridgeError.emptyResponse)
            }
        }
    }
}

private enum AppleLibraryBridgeError: LocalizedError {
    case emptyResponse

    var errorDescription: String? {
        "The local library returned no response."
    }
}
