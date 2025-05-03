cmake_minimum_required(VERSION 3.15)
project(relx VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sanitizer flags
option(ENABLE_SANITIZERS "Enable sanitizers" ON)
if(ENABLE_SANITIZERS)
  # set(SANITIZER_FLAGS "-fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer")
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
  # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
  # set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_FLAGS}")
  message(STATUS "Sanitizers enabled: ${SANITIZER_FLAGS}")
endif()

# Platform specific settings
if(APPLE)
  # On macOS, ensure all targets use libc++
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
endif()

# Find dependencies (Conan or system provided)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(PostgreSQL REQUIRED)

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
  src/connection/postgresql_connection.cpp
  src/connection/postgresql_connection_pool.cpp
  src/connection/postgresql_errors.cpp
  src/connection/postgresql_statement.cpp
  src/connection/postgresql_async_connection.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC include PRIVATE include/relx)
target_link_libraries(${PROJECT_NAME} PUBLIC Boost::boost PostgreSQL::PostgreSQL)

# compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable testing
enable_testing()

# Add tests
add_executable(relx_tests
  test/main_test.cpp
  # Schema tests
  test/schema/fixed_string_test.cpp
  test/schema/column_test.cpp
  test/schema/table_test.cpp
  test/schema/primary_key_test.cpp
  test/schema/foreign_key_test.cpp
  test/schema/index_test.cpp
  test/schema/complex_schema_test.cpp
  test/schema/concepts_test.cpp
  test/schema/custom_column_types_test.cpp
  test/schema/default_value_test.cpp
  test/schema/check_constraint_test.cpp
  test/schema/unique_constraint_test.cpp
  test/schema/optional_column_test.cpp
  test/schema/composite_foreign_key_test.cpp
  test/schema/constraint_alias_test.cpp
  # Query tests
  test/query/query_test.cpp
  test/query/basic_select_test.cpp
  test/query/join_test.cpp
  test/query/condition_test.cpp
  test/query/aggregate_test.cpp
  test/query/case_expression_test.cpp
  test/query/string_function_test.cpp
  test/query/update_query_test.cpp
  test/query/delete_query_test.cpp
  test/query/insert_query_test.cpp
  test/query/select_all_test.cpp
  test/query/distinct_select_test.cpp
  test/query/concise_api_test.cpp
  # Result processing tests
  test/result_test.cpp
  test/advanced_query_test.cpp
  # Connection tests
  test/connection/postgresql_connection_test.cpp
  test/connection/postgresql_api_test.cpp
  test/connection/postgresql_typed_params_test.cpp
  test/connection/postgresql_binary_test.cpp
  test/connection/postgresql_statement_test.cpp
  test/connection/postgresql_connection_pool_test.cpp
  test/connection/dto_mapping_test.cpp
  # PostgreSQL Integration tests
  test/postgres_integration/basic_integration_test.cpp
  test/postgres_integration/schema_integration_test.cpp
  test/postgres_integration/join_integration_test.cpp
  test/postgres_integration/dml_integration_test.cpp
  # test/postgres_integration/connection_pool_integration_test.cpp
  test/postgres_integration/query_integration_test.cpp
  test/postgres_integration/dto_mapping_integration_test.cpp
  test/postgres_integration/async_dto_mapping_test.cpp
  # Additional comprehensive tests
  test/query/edge_case_test.cpp
  test/query/data_type_test.cpp
)

# Link against both gtest and gtest_main libraries
target_link_libraries(relx_tests PRIVATE 
  ${PROJECT_NAME} 
  gtest
  gtest_main
)

# Register test
add_test(NAME relx_tests COMMAND relx_tests)

# Add static_assert tests executable
add_executable(static_assert_tests test/static_assert_tests.cpp)
target_link_libraries(static_assert_tests PRIVATE ${PROJECT_NAME})

# Add examples
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
  message(STATUS "Building examples")
endif()
