add_executable(goodreader_core_tests core_c_api_tests.cpp domain_tests.cpp alignment_tests
                   .cpp application_tests.cpp infrastructure_tests.cpp reader_tests.cpp)

    target_link_libraries(
        goodreader_core_tests PRIVATE goodreader_core goodreader_core_logic GTest::gtest_main)

        target_compile_definitions(
            goodreader_core_tests PRIVATE GOODREADER_FIXTURES_DIR =
                "${CMAKE_CURRENT_SOURCE_DIR}/fixtures" GOODREADER_TEST_DATA_DIR =
                    "${CMAKE_SOURCE_DIR}/test-data")
            goodreader_target_defaults(goodreader_core_tests)

                include(GoogleTest) gtest_discover_tests(goodreader_core_tests)
