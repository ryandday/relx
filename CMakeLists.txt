# Enhanced CMakeLists.txt with modern best practices
cmake_minimum_required(VERSION 3.15...3.29)
project(relx VERSION 0.1.0 LANGUAGES CXX)

option(RELX_ENABLE_TESTS "Enable building tests" OFF)
option(RELX_ENABLE_POSTGRES_CLIENT "Enable PostgreSQL client library" OFF)
option(RELX_ENABLE_INSTALL "Enable installation of library" OFF)
option(RELX_DEV_MODE "Enable development mode" OFF)
option(RELX_ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    
endif()

# Disable C++26 extension warnings, as we will be using C++26 features as they are released
add_compile_options(-Wno-c++26-extensions)

if(RELX_DEV_MODE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set(RELX_ENABLE_TESTS ON)
    set(RELX_ENABLE_POSTGRES_CLIENT ON)
    set(RELX_ENABLE_INSTALL ON)
    
    # Configure libc++ for Clang on both Apple and Linux
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
        
        # On Linux, we may need to link against libc++abi as well
        if(NOT APPLE)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lc++abi")
        endif()
    endif()
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

target_compile_features(relx INTERFACE cxx_std_23)

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
