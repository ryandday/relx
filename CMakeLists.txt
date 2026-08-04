# Enhanced CMakeLists.txt with modern best practices
# 3.25 minimum for cxx_std_26
cmake_minimum_required(VERSION 3.25...3.31)
project(relx VERSION 0.1.0 LANGUAGES CXX)

option(RELX_ENABLE_TESTS "Enable building tests" OFF)
option(RELX_ENABLE_POSTGRES_CLIENT "Enable PostgreSQL client library" OFF)
option(RELX_ENABLE_INSTALL "Enable installation of library" OFF)
option(RELX_DEV_MODE "Enable development mode" OFF)
option(RELX_ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 26)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# C++26 reflection (P2996) requires GCC 16.1+ with -freflection.
# Clang has no reflection support yet (expected ~Clang 24); see docs/cpp26-reflection-plan.md
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.1)
    message(FATAL_ERROR "relx requires GCC 16.1+ for C++26 reflection (found GCC ${CMAKE_CXX_COMPILER_VERSION})")
elseif(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "relx requires GCC 16.1+ for C++26 reflection; ${CMAKE_CXX_COMPILER_ID} does not implement P2996 yet")
endif()

if(RELX_DEV_MODE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(RELX_ENABLE_TESTS ON)
    set(RELX_ENABLE_POSTGRES_CLIENT ON)
    set(RELX_ENABLE_INSTALL ON)
    
endif()

# Code coverage configuration
if(RELX_ENABLE_COVERAGE)
    include(cmake/CodeCoverage.cmake)
    check_coverage_support()
    enable_coverage_globally()
    coverage_info()
endif()

# Find dependencies (Conan or system provided)
find_package(Boost REQUIRED COMPONENTS system thread)

# Create header-only library target
add_library(relx INTERFACE)

# Create alias target for consistent usage (internal vs external)
add_library(relx::relx ALIAS relx)

target_include_directories(relx INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(relx INTERFACE
    Boost::system
    Boost::thread
)

target_compile_features(relx INTERFACE cxx_std_26)

# libstdc++'s <meta> is silently empty without -freflection, so consumers must inherit it
target_compile_options(relx INTERFACE $<$<CXX_COMPILER_ID:GNU>:-freflection>)

add_subdirectory(src)

if(RELX_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
    
    # Setup coverage target if coverage is enabled
    if(RELX_ENABLE_COVERAGE)
        setup_target_for_coverage_lcov(
            NAME coverage
            EXECUTABLE ${CMAKE_CTEST_COMMAND}
            DEPENDENCIES relx_tests
            EXCLUDE
                '*/test/*'
                '*/tests/*'
                '*/googletest/*'
                '*/boost/*'
                '*/build/*'
                '*/CMakeFiles/*'
                '/usr/*'
                '*/_deps/*'
        )
    endif()
endif()

if(RELX_ENABLE_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install the header-only interface library
    install(TARGETS relx
        EXPORT relxTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install headers
    install(DIRECTORY include/ 
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
    )

    # Create and install package config files
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/relxConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/relxConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/relxConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/relx
    )

    # Install package config files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/relxConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/relxConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/relx
    )

    # Install targets
    install(EXPORT relxTargets
        FILE relxTargets.cmake
        NAMESPACE relx::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/relx
    )
endif()
