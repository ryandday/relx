cmake_minimum_required(VERSION 3.15)
project(sqllib VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Add library (header-only)
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)
target_link_libraries(${PROJECT_NAME} INTERFACE Boost::boost)

# compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add executable for the main app
add_executable(sqllib_main src/main.cpp)
target_link_libraries(sqllib_main PRIVATE ${PROJECT_NAME} fmt::fmt)

# Add examples
add_executable(schema_example examples/schema_example.cpp)
target_link_libraries(schema_example PRIVATE ${PROJECT_NAME})

# Enable testing
enable_testing()

# Add tests
add_executable(sqllib_tests
  tests/main_test.cpp
  tests/fixed_string_test.cpp
  tests/column_test.cpp
  tests/table_test.cpp
  tests/primary_key_test.cpp
  tests/foreign_key_test.cpp
  tests/index_test.cpp
  tests/complex_schema_test.cpp
  tests/concepts_test.cpp
  tests/custom_column_types_test.cpp
)

# Link against both gtest and gtest_main libraries
target_link_libraries(sqllib_tests PRIVATE 
  ${PROJECT_NAME} 
  gtest
  gtest_main
)

# Register test
add_test(NAME sqllib_tests COMMAND sqllib_tests)

# Add a simple test executable for concepts without external dependencies
add_executable(basic_concepts_test tests/basic_concepts_test.cpp)
target_link_libraries(basic_concepts_test PRIVATE ${PROJECT_NAME})

# Add static_assert tests executable
add_executable(static_assert_tests tests/static_assert_tests.cpp)
target_link_libraries(static_assert_tests PRIVATE ${PROJECT_NAME})

# Register static_assert test
add_test(NAME static_assert_tests COMMAND static_assert_tests)