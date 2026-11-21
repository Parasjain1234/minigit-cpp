include(FetchContent)

find_package(GTest QUIET)
if(GTest_FOUND)
    set(MINIGIT_GTEST_MAIN GTest::gtest_main)
else()
    message(STATUS "GoogleTest not installed locally, fetching v1.14.0")
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set(MINIGIT_GTEST_MAIN gtest_main)
endif()

add_executable(sha1_test sha1_test.cpp)
target_link_libraries(sha1_test PRIVATE minigit_core ${MINIGIT_GTEST_MAIN})
add_test(NAME sha1_test COMMAND sha1_test)

add_executable(object_store_test object_store_test.cpp)
target_link_libraries(object_store_test PRIVATE minigit_core ${MINIGIT_GTEST_MAIN})
add_test(NAME object_store_test COMMAND object_store_test)

add_executable(repository_test repository_test.cpp)
target_link_libraries(repository_test PRIVATE minigit_core ${MINIGIT_GTEST_MAIN})
add_test(NAME repository_test COMMAND repository_test)

add_executable(history_test history_test.cpp)
target_link_libraries(history_test PRIVATE minigit_core ${MINIGIT_GTEST_MAIN})
add_test(NAME history_test COMMAND history_test)

add_executable(graph_test graph_test.cpp)
target_link_libraries(graph_test PRIVATE minigit_core ${MINIGIT_GTEST_MAIN})
add_test(NAME graph_test COMMAND graph_test)

add_executable(branch_test branch_test.cpp)
target_link_libraries(branch_test PRIVATE minigit_core ${MINIGIT_GTEST_MAIN})
add_test(NAME branch_test COMMAND branch_test)

add_executable(lcs_test lcs_test.cpp)
target_link_libraries(lcs_test PRIVATE minigit_core ${MINIGIT_GTEST_MAIN})
add_test(NAME lcs_test COMMAND lcs_test)

add_executable(myers_test myers_test.cpp)
target_link_libraries(myers_test PRIVATE minigit_core ${MINIGIT_GTEST_MAIN})
add_test(NAME myers_test COMMAND myers_test)

add_executable(unified_test unified_test.cpp)
target_link_libraries(unified_test PRIVATE minigit_core ${MINIGIT_GTEST_MAIN})
add_test(NAME unified_test COMMAND unified_test)

add_executable(diff_repository_test diff_repository_test.cpp)
target_link_libraries(diff_repository_test PRIVATE minigit_core ${MINIGIT_GTEST_MAIN})
add_test(NAME diff_repository_test COMMAND diff_repository_test)
