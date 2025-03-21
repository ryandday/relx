cmake_minimum_required(VERSION 3.15)
project(sqllib VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies (Conan-provided)
find_package(fmt REQUIRED)

# Add library (header-only for now)
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)

# Add executable for the main app
add_executable(sqllib_main src/main.cpp)
target_link_libraries(sqllib_main PRIVATE ${PROJECT_NAME} fmt::fmt)

# Add examples
add_executable(schema_example examples/schema_example.cpp)
target_link_libraries(schema_example PRIVATE ${PROJECT_NAME})