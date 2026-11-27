#ifndef GOODREADER_CORE_C_API_H
#define GOODREADER_CORE_C_API_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(GOODREADER_C_API_BUILD)
#define GR_API __declspec(dllexport)
#else
#define GR_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define GR_API __attribute__((visibility("default")))
#else
#define GR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle to one GoodReader core instance. */
typedef struct gr_core gr_core_t;
/** Opaque handle to parsed, reflowable book content. */
typedef struct gr_book_document gr_book_document_t;
/** Opaque handle to a validated word/time alignment index. */
typedef struct gr_alignment_index gr_alignment_index_t;

/** Opaque handle used to request cancellation of an asynchronous operation. */
typedef struct gr_operation gr_operation_t;

/** Stable result codes returned by every synchronous C API function. */
typedef enum gr_status {
    GR_STATUS_OK = 0,
    GR_STATUS_INVALID_ARGUMENT = 1,
    GR_STATUS_UNSUPPORTED_FORMAT = 2,
    GR_STATUS_NOT_FOUND = 3,
    GR_STATUS_IO_ERROR = 4,
    GR_STATUS_CANCELLED = 5,
    GR_STATUS_INTERNAL_ERROR = 6,
    GR_STATUS_CORRUPT_BOOK = 7,
    GR_STATUS_NO_TEXT_LAYER = 8,
    GR_STATUS_BUFFER_TOO_SMALL = 9
} gr_status_t;

/** Stable location within a chapter, independent of screen pagination. */
typedef struct gr_text_position {
    size_t chapter_index;
    size_t word_index;
} gr_text_position_t;

/** One monotonic word-to-audiobook-time synchronization anchor. */
typedef struct gr_alignment_anchor {
    size_t global_word_index;
    int64_t audio_time_millis;
    double confidence;
} gr_alignment_anchor_t;

/** Supported text document formats. */
typedef enum gr_book_format {
    GR_BOOK_FORMAT_UNKNOWN = 0,
    GR_BOOK_FORMAT_PDF = 1,
    GR_BOOK_FORMAT_EPUB = 2,
    GR_BOOK_FORMAT_FB2 = 3
} gr_book_format_t;

/**
 * Completion callback for asynchronous book-file validation.
 *
 * The detail string is valid only for the duration of the callback. Keep
 * user_data alive until the callback has returned, including after core
 * destruction or operation cancellation. C++ callbacks must not throw
 * exceptions across the ABI boundary.
 */
typedef void (*gr_validation_callback_t)(gr_status_t status, gr_book_format_t format,
                                         const char* detail, void* user_data);

/** Returns the ABI version string for diagnostics. */
GR_API const char* gr_core_version(void);

/** Creates a core instance. On success, out_core receives an owned handle. */
GR_API gr_status_t gr_core_create(gr_core_t** out_core);

/**
 * Cancels outstanding operations and releases the core handle.
 *
 * Existing operations may still invoke their callbacks with GR_STATUS_CANCELLED.
 */
GR_API void gr_core_destroy(gr_core_t* core);

/**
 * Validates that a path exists as a regular PDF, EPUB, or FB2 file.
 *
 * The path is interpreted as UTF-8. out_format must be non-null and is set to
 * GR_BOOK_FORMAT_UNKNOWN when validation fails.
 */
GR_API gr_status_t gr_core_validate_book_file(const gr_core_t* core, const char* path_utf8,
                                              gr_book_format_t* out_format);

/**
 * Opens and parses a local EPUB, FB2, or text-based PDF into reflowable chapters.
 *
 * A scanned PDF returns GR_STATUS_NO_TEXT_LAYER. On success, out_document
 * receives an owned handle that must be released with gr_book_document_destroy.
 */
GR_API gr_status_t gr_core_open_book_file(const gr_core_t* core, const char* path_utf8,
                                          gr_book_document_t** out_document, char* detail_utf8,
                                          size_t detail_capacity, size_t* detail_required);

/** Releases parsed document memory. */
GR_API void gr_book_document_destroy(gr_book_document_t* document);

/** Returns the document title and author into caller-owned UTF-8 buffers. */
GR_API gr_status_t gr_book_document_copy_metadata(const gr_book_document_t* document,
                                                  char* title_utf8, size_t title_capacity,
                                                  size_t* title_required, char* author_utf8,
                                                  size_t author_capacity, size_t* author_required);

/** Returns the total chapter and word counts. */
GR_API gr_status_t gr_book_document_get_counts(const gr_book_document_t* document,
                                               size_t* chapter_count, size_t* word_count);

/** Returns a chapter title and its word count. */
GR_API gr_status_t gr_book_document_copy_chapter_title(const gr_book_document_t* document,
                                                       size_t chapter_index, char* title_utf8,
                                                       size_t title_capacity,
                                                       size_t* title_required, size_t* word_count);

/**
 * Copies text covering [first_word_index, first_word_index + max_words).
 * required_bytes includes the trailing NUL. A zero-capacity call can be used
 * to query the required buffer size.
 */
GR_API gr_status_t gr_book_document_copy_word_range(const gr_book_document_t* document,
                                                    size_t chapter_index, size_t first_word_index,
                                                    size_t max_words, char* text_utf8,
                                                    size_t text_capacity, size_t* required_bytes);

/** Converts between a global word offset and the stable chapter/word position. */
GR_API gr_status_t gr_book_document_position_for_global_word(const gr_book_document_t* document,
                                                             size_t global_word_index,
                                                             gr_text_position_t* out_position);
GR_API gr_status_t gr_book_document_global_word_index(const gr_book_document_t* document,
                                                      gr_text_position_t position,
                                                      size_t* out_global_word_index);

/** Creates an alignment index from increasing global-word/audio-time anchors. */
GR_API gr_status_t gr_alignment_index_create(const gr_alignment_anchor_t* anchors,
                                             size_t anchor_count, gr_alignment_index_t** out_index);
GR_API void gr_alignment_index_destroy(gr_alignment_index_t* index);

/** Converts cumulative audiobook time to a stable chapter/word position. */
GR_API gr_status_t gr_book_document_position_at_audio_time(const gr_book_document_t* document,
                                                           const gr_alignment_index_t* index,
                                                           int64_t audio_time_millis,
                                                           gr_text_position_t* out_position);

/** Converts a stable chapter/word position to cumulative audiobook time. */
GR_API gr_status_t gr_book_document_audio_time_for_position(const gr_book_document_t* document,
                                                            const gr_alignment_index_t* index,
                                                            gr_text_position_t position,
                                                            int64_t* out_audio_time_millis);

/**
 * Starts asynchronous book-file validation.
 *
 * out_operation receives an owned cancellation handle. The callback may run
 * before this function returns. Release the handle with gr_operation_release;
 * release does not cancel the operation.
 */
GR_API gr_status_t gr_core_validate_book_file_async(gr_core_t* core, const char* path_utf8,
                                                    gr_validation_callback_t callback,
                                                    void* user_data,
                                                    gr_operation_t** out_operation);

/** Requests cancellation of an asynchronous operation. */
GR_API gr_status_t gr_operation_cancel(gr_operation_t* operation);

/** Releases an operation handle without waiting for its callback. */
GR_API void gr_operation_release(gr_operation_t* operation);

#ifdef __cplusplus
}
#endif

#endif
