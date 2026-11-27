add_library(goodreader_core SHARED
    src/core_c_api.cpp
)

target_include_directories(goodreader_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(goodreader_core PUBLIC goodreader_core_logic)
target_compile_definitions(goodreader_core PRIVATE GOODREADER_C_API_BUILD)
set_target_properties(goodreader_core PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
goodreader_target_defaults(goodreader_core)

if(WIN32)
    target_compile_definitions(goodreader_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
