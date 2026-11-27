find_package(date CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(TagLib CONFIG REQUIRED)
find_package(utf8proc CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_library(goodreader_core_logic STATIC
    library/filesystem_book_importer.cpp
    library/local_services.cpp
    library/ports.cpp
    library/types.cpp
    library/use_cases.cpp
    reader/document.cpp
    stats/streak.cpp
    storage/sqlite_library_repository.cpp
    sync/find_text_match.cpp
    sync/search.cpp
    sync/text_normalizer.cpp
)

target_include_directories(goodreader_core_logic
    PUBLIC
        library/include
        reader/include
        sync/include
        storage/include
)
target_link_libraries(goodreader_core_logic
    PUBLIC
        date::date-tz
        SQLite3::SQLite3
        TagLib::TagLib
        utf8proc::utf8proc
        ZLIB::ZLIB
)
goodreader_target_defaults(goodreader_core_logic)
