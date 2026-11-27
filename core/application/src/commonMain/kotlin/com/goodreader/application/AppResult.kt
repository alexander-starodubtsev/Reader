package com.goodreader.application

/** Result of an application operation, with failures represented explicitly. */
sealed interface AppResult<out T> {
    /** A successful operation. */
    data class Success<T>(
        val value: T,
    ) : AppResult<T>

    /** A failed operation with a typed, user-independent error. */
    data class Failure(
        val error: AppError,
    ) : AppResult<Nothing>
}

/** Stable error categories exposed by application use cases and ports. */
sealed interface AppError {
    /** Input was invalid or unsupported. */
    data class Validation(
        val code: String,
        val detail: String,
    ) : AppError

    /** A requested library entity does not exist. */
    data class NotFound(
        val entity: String,
        val id: String,
    ) : AppError

    /** An external dependency could not complete its operation. */
    data class Unavailable(
        val dependency: String,
        val detail: String,
    ) : AppError

    /** The operation was cancelled by the caller. */
    data object Cancelled : AppError

    /** An unexpected failure without sensitive implementation details. */
    data class Unexpected(
        val operation: String,
    ) : AppError
}
