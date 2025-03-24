cmake_minimum_required(VERSION 3.15)
project(sqllib VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sanitizer flags
option(ENABLE_SANITIZERS "Enable sanitizers" ON)
if(ENABLE_SANITIZERS)
  set(SANITIZER_FLAGS "-fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_FLAGS}")
  message(STATUS "Sanitizers enabled: ${SANITIZER_FLAGS}")
endif()

# Platform specific settings
if(APPLE)
  # On macOS, ensure all targets use libc++
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
endif()

# Find dependencies (Conan-provided)
find_package(fmt REQUIRED)
find_package(Boost REQUIRED)
find_package(SQLite3 REQUIRED)

# Set up Google Test with FetchContent
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.16.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Add library
add_library(${PROJECT_NAME} 
  src/connection/sqlite_connection.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC include PRIVATE include/sqllib)
target_link_libraries(${PROJECT_NAME} PUBLIC Boost::boost SQLite::SQLite3)

# compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable testing
enable_testing()

# Add tests
add_executable(sqllib_tests
  tests/main_test.cpp
  # Schema tests
  tests/schema/fixed_string_test.cpp
  tests/schema/column_test.cpp
  tests/schema/table_test.cpp
  tests/schema/primary_key_test.cpp
  tests/schema/foreign_key_test.cpp
  tests/schema/index_test.cpp
  tests/schema/complex_schema_test.cpp
  tests/schema/concepts_test.cpp
  tests/schema/custom_column_types_test.cpp
  tests/schema/default_value_test.cpp
  tests/schema/check_constraint_test.cpp
  tests/schema/unique_constraint_test.cpp
  tests/schema/optional_column_test.cpp
  tests/schema/composite_foreign_key_test.cpp
  tests/schema/constraint_alias_test.cpp
  # Query tests
  tests/query/query_test.cpp
  tests/query/basic_select_test.cpp
  tests/query/join_test.cpp
  tests/query/condition_test.cpp
  tests/query/aggregate_test.cpp
  tests/query/case_expression_test.cpp
  tests/query/string_function_test.cpp
  tests/query/update_query_test.cpp
  tests/query/delete_query_test.cpp
  tests/query/insert_query_test.cpp
  # Result processing tests
  tests/result_test.cpp
  tests/advanced_query_test.cpp
  # Connection tests
  tests/connection/sqlite_connection_test.cpp
  # Additional comprehensive tests
  tests/query/edge_case_test.cpp
  tests/query/data_type_test.cpp
#   tests/break_the_library_test.cpp
)

# Link against both gtest and gtest_main libraries
target_link_libraries(sqllib_tests PRIVATE 
  ${PROJECT_NAME} 
  gtest
  gtest_main
)

# Register test
add_test(NAME sqllib_tests COMMAND sqllib_tests)

# Add static_assert tests executable
add_executable(static_assert_tests tests/static_assert_tests.cpp)
target_link_libraries(static_assert_tests PRIVATE ${PROJECT_NAME})
