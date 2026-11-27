#include "goodreader/core_c_api.h"

#include <jni.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

bool read_path(JNIEnv* environment, jbyteArray path_utf8, std::string& path) {
    if (path_utf8 == nullptr)
        return false;
    const auto length = environment->GetArrayLength(path_utf8);
    if (environment->ExceptionCheck() == JNI_TRUE || length <= 0)
        return false;
    std::vector<jbyte> bytes(static_cast<std::size_t>(length));
    environment->GetByteArrayRegion(path_utf8, 0, length, bytes.data());
    if (environment->ExceptionCheck() == JNI_TRUE)
        return false;
    path.reserve(bytes.size());
    for (const auto byte : bytes) {
        path.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
    }
    return true;
}

jbyteArray to_byte_array(JNIEnv* environment, const char* text, std::size_t length) {
    if (length > static_cast<std::size_t>(std::numeric_limits<jsize>::max()))
        return nullptr;
    auto result = environment->NewByteArray(static_cast<jsize>(length));
    if (result != nullptr && length > 0) {
        environment->SetByteArrayRegion(result, 0, static_cast<jsize>(length),
                                        reinterpret_cast<const jbyte*>(text));
    }
    return result;
}

gr_book_document_t* document_from_handle(jlong handle) {
    return reinterpret_cast<gr_book_document_t*>(static_cast<std::uintptr_t>(handle));
}

gr_alignment_index_t* alignment_from_handle(jlong handle) {
    return reinterpret_cast<gr_alignment_index_t*>(static_cast<std::uintptr_t>(handle));
}

jlong handle_from_pointer(const void* pointer) {
    return static_cast<jlong>(reinterpret_cast<std::uintptr_t>(pointer));
}

jintArray position_to_int_array(JNIEnv* environment, gr_text_position_t position) {
    if (position.chapter_index > static_cast<std::size_t>(std::numeric_limits<jint>::max()) ||
        position.word_index > static_cast<std::size_t>(std::numeric_limits<jint>::max())) {
        return nullptr;
    }
    auto result = environment->NewIntArray(2);
    if (result != nullptr) {
        const std::array<jint, 2> values{static_cast<jint>(position.chapter_index),
                                         static_cast<jint>(position.word_index)};
        environment->SetIntArrayRegion(result, 0, 2, values.data());
    }
    return result;
}

jbyteArray copy_chapter_title(JNIEnv* environment, gr_book_document_t* document,
                              std::size_t chapter_index) {
    std::size_t required = 0;
    std::size_t word_count = 0;
    const auto status = gr_book_document_copy_chapter_title(document, chapter_index, nullptr, 0,
                                                            &required, &word_count);
    if (status != GR_STATUS_BUFFER_TOO_SMALL || required == 0)
        return nullptr;
    std::vector<char> title(required);
    if (gr_book_document_copy_chapter_title(document, chapter_index, title.data(), title.size(),
                                            &required, &word_count) != GR_STATUS_OK) {
        return nullptr;
    }
    return to_byte_array(environment, title.data(), required - 1);
}

} // namespace

extern "C" JNIEXPORT jint JNICALL Java_com_goodreader_android_NativeCore_validateBookFile(
    JNIEnv* environment, jclass, jbyteArray path_utf8) {
    try {
        std::string path;
        if (!read_path(environment, path_utf8, path))
            return static_cast<jint>(GR_STATUS_INVALID_ARGUMENT);

        gr_core_t* core = nullptr;
        const auto create_status = gr_core_create(&core);
        if (create_status != GR_STATUS_OK) {
            return static_cast<jint>(create_status);
        }

        gr_book_format_t format = GR_BOOK_FORMAT_UNKNOWN;
        const auto status = gr_core_validate_book_file(core, path.c_str(), &format);
        gr_core_destroy(core);
        return static_cast<jint>(status);
    } catch (...) {
        return static_cast<jint>(GR_STATUS_INTERNAL_ERROR);
    }
}

extern "C" JNIEXPORT jlong JNICALL Java_com_goodreader_android_NativeCore_openBookDocument(
    JNIEnv* environment, jclass, jbyteArray path_utf8) {
    try {
        std::string path;
        if (!read_path(environment, path_utf8, path)) {
            jclass exception = environment->FindClass("java/io/IOException");
            if (exception != nullptr)
                environment->ThrowNew(exception, "The book path is invalid.");
            return 0;
        }

        gr_core_t* core = nullptr;
        if (gr_core_create(&core) != GR_STATUS_OK) {
            jclass exception = environment->FindClass("java/io/IOException");
            if (exception != nullptr)
                environment->ThrowNew(exception, "The reader could not start.");
            return 0;
        }
        gr_book_document_t* document = nullptr;
        std::array<char, 512> detail{};
        std::size_t detail_required = 0;
        const auto status = gr_core_open_book_file(core, path.c_str(), &document, detail.data(),
                                                   detail.size(), &detail_required);
        gr_core_destroy(core);
        if (status != GR_STATUS_OK || document == nullptr) {
            jclass exception = environment->FindClass("java/io/IOException");
            if (exception != nullptr) {
                const auto message =
                    detail[0] == '\0' ? "The book could not be opened." : detail.data();
                environment->ThrowNew(exception, message);
            }
            return 0;
        }
        return static_cast<jlong>(reinterpret_cast<std::uintptr_t>(document));
    } catch (...) {
        jclass exception = environment->FindClass("java/io/IOException");
        if (exception != nullptr)
            environment->ThrowNew(exception, "The book could not be opened.");
        return 0;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_goodreader_android_NativeCore_closeBookDocument(JNIEnv*, jclass, jlong handle) {
    gr_book_document_destroy(document_from_handle(handle));
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_goodreader_android_NativeCore_documentTitle(JNIEnv* environment, jclass, jlong handle) {
    auto* document = document_from_handle(handle);
    if (document == nullptr)
        return nullptr;
    std::size_t title_required = 0;
    std::size_t author_required = 0;
    if (gr_book_document_copy_metadata(document, nullptr, 0, &title_required, nullptr, 0,
                                       &author_required) != GR_STATUS_BUFFER_TOO_SMALL ||
        title_required == 0) {
        return nullptr;
    }
    std::vector<char> title(title_required);
    std::vector<char> author(author_required);
    if (gr_book_document_copy_metadata(document, title.data(), title.size(), &title_required,
                                       author.data(), author.size(),
                                       &author_required) != GR_STATUS_OK) {
        return nullptr;
    }
    return to_byte_array(environment, title.data(), title_required - 1);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_goodreader_android_NativeCore_documentChapterCount(JNIEnv*, jclass, jlong handle) {
    std::size_t chapters = 0;
    std::size_t words = 0;
    return gr_book_document_get_counts(document_from_handle(handle), &chapters, &words) ==
                   GR_STATUS_OK
               ? static_cast<jint>(chapters)
               : 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_goodreader_android_NativeCore_documentWordCount(JNIEnv*, jclass, jlong handle) {
    std::size_t chapters = 0;
    std::size_t words = 0;
    return gr_book_document_get_counts(document_from_handle(handle), &chapters, &words) ==
                   GR_STATUS_OK
               ? static_cast<jint>(words)
               : 0;
}

extern "C" JNIEXPORT jbyteArray JNICALL Java_com_goodreader_android_NativeCore_documentChapterTitle(
    JNIEnv* environment, jclass, jlong handle, jint chapter_index) {
    if (chapter_index < 0)
        return nullptr;
    return copy_chapter_title(environment, document_from_handle(handle),
                              static_cast<std::size_t>(chapter_index));
}

extern "C" JNIEXPORT jint JNICALL Java_com_goodreader_android_NativeCore_documentChapterWordCount(
    JNIEnv*, jclass, jlong handle, jint chapter_index) {
    if (chapter_index < 0)
        return 0;
    std::size_t required = 0;
    std::size_t word_count = 0;
    const auto status = gr_book_document_copy_chapter_title(document_from_handle(handle),
                                                            static_cast<std::size_t>(chapter_index),
                                                            nullptr, 0, &required, &word_count);
    return status == GR_STATUS_BUFFER_TOO_SMALL ? static_cast<jint>(word_count) : 0;
}

extern "C" JNIEXPORT jbyteArray JNICALL Java_com_goodreader_android_NativeCore_documentTextRange(
    JNIEnv* environment, jclass, jlong handle, jint chapter_index, jint first_word,
    jint max_words) {
    if (chapter_index < 0 || first_word < 0 || max_words < 0)
        return nullptr;
    std::size_t required = 0;
    const auto status = gr_book_document_copy_word_range(
        document_from_handle(handle), static_cast<std::size_t>(chapter_index),
        static_cast<std::size_t>(first_word), static_cast<std::size_t>(max_words), nullptr, 0,
        &required);
    if (status != GR_STATUS_BUFFER_TOO_SMALL || required == 0)
        return nullptr;
    std::vector<char> text(required);
    if (gr_book_document_copy_word_range(
            document_from_handle(handle), static_cast<std::size_t>(chapter_index),
            static_cast<std::size_t>(first_word), static_cast<std::size_t>(max_words), text.data(),
            text.size(), &required) != GR_STATUS_OK) {
        return nullptr;
    }
    return to_byte_array(environment, text.data(), required - 1);
}

extern "C" JNIEXPORT jintArray JNICALL Java_com_goodreader_android_NativeCore_positionForGlobalWord(
    JNIEnv* environment, jclass, jlong handle, jint global_word_index) {
    if (global_word_index < 0)
        return nullptr;
    gr_text_position_t position{};
    if (gr_book_document_position_for_global_word(document_from_handle(handle),
                                                  static_cast<std::size_t>(global_word_index),
                                                  &position) != GR_STATUS_OK ||
        position.chapter_index > static_cast<std::size_t>(std::numeric_limits<jint>::max()) ||
        position.word_index > static_cast<std::size_t>(std::numeric_limits<jint>::max())) {
        return nullptr;
    }
    return position_to_int_array(environment, position);
}

extern "C" JNIEXPORT jlong JNICALL Java_com_goodreader_android_NativeCore_createAlignmentIndex(
    JNIEnv* environment, jclass, jlongArray global_word_indices, jlongArray audio_times_millis,
    jdoubleArray confidences) {
    if (global_word_indices == nullptr || audio_times_millis == nullptr || confidences == nullptr) {
        return 0;
    }
    const auto count = environment->GetArrayLength(global_word_indices);
    if (environment->ExceptionCheck() == JNI_TRUE || count < 2 ||
        environment->GetArrayLength(audio_times_millis) != count ||
        environment->GetArrayLength(confidences) != count) {
        return 0;
    }

    std::vector<jlong> words(static_cast<std::size_t>(count));
    std::vector<jlong> times(static_cast<std::size_t>(count));
    std::vector<jdouble> scores(static_cast<std::size_t>(count));
    environment->GetLongArrayRegion(global_word_indices, 0, count, words.data());
    environment->GetLongArrayRegion(audio_times_millis, 0, count, times.data());
    environment->GetDoubleArrayRegion(confidences, 0, count, scores.data());
    if (environment->ExceptionCheck() == JNI_TRUE)
        return 0;

    std::vector<gr_alignment_anchor_t> anchors;
    anchors.reserve(static_cast<std::size_t>(count));
    for (jsize i = 0; i < count; ++i) {
        if (words[static_cast<std::size_t>(i)] < 0 ||
            static_cast<std::uint64_t>(words[static_cast<std::size_t>(i)]) >
                std::numeric_limits<std::size_t>::max()) {
            return 0;
        }
        anchors.push_back({
            static_cast<std::size_t>(words[static_cast<std::size_t>(i)]),
            static_cast<std::int64_t>(times[static_cast<std::size_t>(i)]),
            scores[static_cast<std::size_t>(i)],
        });
    }

    gr_alignment_index_t* index = nullptr;
    if (gr_alignment_index_create(anchors.data(), anchors.size(), &index) != GR_STATUS_OK) {
        return 0;
    }
    return handle_from_pointer(index);
}

extern "C" JNIEXPORT void JNICALL
Java_com_goodreader_android_NativeCore_closeAlignmentIndex(JNIEnv*, jclass, jlong handle) {
    gr_alignment_index_destroy(alignment_from_handle(handle));
}

extern "C" JNIEXPORT jintArray JNICALL Java_com_goodreader_android_NativeCore_positionAtAudioTime(
    JNIEnv* environment, jclass, jlong document_handle, jlong alignment_handle,
    jlong audio_time_millis) {
    gr_text_position_t position{};
    if (gr_book_document_position_at_audio_time(
            document_from_handle(document_handle), alignment_from_handle(alignment_handle),
            static_cast<std::int64_t>(audio_time_millis), &position) != GR_STATUS_OK) {
        return nullptr;
    }
    return position_to_int_array(environment, position);
}

extern "C" JNIEXPORT jlong JNICALL Java_com_goodreader_android_NativeCore_audioTimeForPosition(
    JNIEnv*, jclass, jlong document_handle, jlong alignment_handle, jint chapter_index,
    jint word_index) {
    if (chapter_index < 0 || word_index < 0)
        return -1;
    std::int64_t audio_time_millis = -1;
    const gr_text_position_t position{
        static_cast<std::size_t>(chapter_index),
        static_cast<std::size_t>(word_index),
    };
    if (gr_book_document_audio_time_for_position(document_from_handle(document_handle),
                                                 alignment_from_handle(alignment_handle), position,
                                                 &audio_time_millis) != GR_STATUS_OK) {
        return -1;
    }
    return static_cast<jlong>(audio_time_millis);
}
