#include "goodreader/application/ports.hpp"

#include <stdexcept>
#include <utility>

namespace goodreader::application {
namespace {

void require_non_blank(const std::string& value, const char* message) {
    if (value.find_first_not_of(" \t\n\r\f\v") == std::string::npos) {
        throw std::invalid_argument(message);
    }
}

} // namespace

RecognizedSpeech::RecognizedSpeech(std::string text, std::int64_t start_millis,
                                   std::int64_t end_millis, std::string engine_version)
    : text(std::move(text)), start_millis(start_millis), end_millis(end_millis),
      engine_version(std::move(engine_version)) {
    require_non_blank(this->text, "Recognized text must not be blank.");
    if (this->start_millis < 0 || this->end_millis <= this->start_millis) {
        throw std::invalid_argument("Speech timestamps are invalid.");
    }
    require_non_blank(this->engine_version, "ASR engine version must not be blank.");
}

ConversionProgress::ConversionProgress(double completed) : completed(completed) {
    if (this->completed < 0.0 || this->completed > 1.0) {
        throw std::invalid_argument("Conversion progress must be between zero and one.");
    }
}

LocalUser::LocalUser(std::string id, bool is_local_only)
    : id(std::move(id)), is_local_only(is_local_only) {
    require_non_blank(this->id, "User ID must not be blank.");
}

} // namespace goodreader::application
