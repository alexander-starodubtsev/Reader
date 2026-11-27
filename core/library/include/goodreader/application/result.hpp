#pragma once

#include <string>
#include <utility>
#include <variant>

namespace goodreader::application {

/** A validation failure safe to display without exposing implementation details. */
struct ValidationError {
    std::string code;
    std::string detail;
    bool operator==(const ValidationError&) const = default;
};

/** A requested library entity does not exist. */
struct NotFoundError {
    std::string entity;
    std::string id;
    bool operator==(const NotFoundError&) const = default;
};

/** An external dependency could not complete its operation. */
struct UnavailableError {
    std::string dependency;
    std::string detail;
    bool operator==(const UnavailableError&) const = default;
};

/** The operation was cancelled by the caller. */
struct CancelledError {
    bool operator==(const CancelledError&) const = default;
};

/** An unexpected failure without sensitive implementation details. */
struct UnexpectedError {
    std::string operation;
    bool operator==(const UnexpectedError&) const = default;
};

using AppError =
    std::variant<ValidationError, NotFoundError, UnavailableError, CancelledError, UnexpectedError>;

/** Explicit success or typed failure returned from application operations. */
template <typename T> class AppResult {
  public:
    /** Constructs a successful result containing the supplied value. */
    static AppResult success(T value) { return AppResult(std::move(value)); }

    /** Constructs a failed result containing a typed application error. */
    static AppResult failure(AppError error) { return AppResult(std::move(error)); }

    /** Reports whether this result contains a value. */
    bool has_value() const noexcept { return std::holds_alternative<T>(value_); }

    /** Provides explicit boolean testing for successful results. */
    explicit operator bool() const noexcept { return has_value(); }

    /** Returns the successful value; callers must first check has_value(). */
    T& value() { return std::get<T>(value_); }

    /** Returns the successful value; callers must first check has_value(). */
    const T& value() const { return std::get<T>(value_); }

    /** Returns the failure; callers must first check that this result failed. */
    AppError& error() { return std::get<AppError>(value_); }

    /** Returns the failure; callers must first check that this result failed. */
    const AppError& error() const { return std::get<AppError>(value_); }

    bool operator==(const AppResult&) const = default;

  private:
    explicit AppResult(T value) : value_(std::move(value)) {}
    explicit AppResult(AppError error) : value_(std::move(error)) {}

    std::variant<T, AppError> value_;
};

} // namespace goodreader::application
