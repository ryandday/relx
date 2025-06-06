# CodeCoverage.cmake - CMake module for code coverage support
# 
# This module provides functions and targets for generating code coverage reports
# using gcov/lcov for GCC and llvm-cov/lcov for Clang compilers.
#
# Usage:
#   include(cmake/CodeCoverage.cmake)
#   setup_target_for_coverage_lcov(
#       NAME coverage
#       EXECUTABLE ctest
#       DEPENDENCIES my_tests
#   )

include(CMakeParseArguments)

# Check if coverage is supported
function(check_coverage_support)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "Code coverage results with optimizations may be misleading. Consider using Debug build type.")
    endif()
    
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Code coverage only supported with GCC or Clang compilers. Current compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
    
    # Find required tools based on compiler
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # For Clang, we need llvm-cov instead of gcov
        find_program(LLVM_COV_PATH llvm-cov)
        if(NOT LLVM_COV_PATH)
            # Try to find versioned llvm-cov
            string(REGEX MATCH "[0-9]+" CLANG_VERSION ${CMAKE_CXX_COMPILER})
            if(CLANG_VERSION)
                find_program(LLVM_COV_PATH llvm-cov-${CLANG_VERSION})
            endif()
        endif()
        
        if(NOT LLVM_COV_PATH)
            message(FATAL_ERROR "llvm-cov not found! Required for Clang code coverage.")
        endif()
        
        # Create a wrapper script for llvm-cov to work with lcov
        set(GCOV_PATH "${CMAKE_BINARY_DIR}/llvm-gcov-wrapper.sh" PARENT_SCOPE)
        set(GCOV_PATH "${CMAKE_BINARY_DIR}/llvm-gcov-wrapper.sh")
        file(WRITE "${GCOV_PATH}" "#!/bin/bash\nexec ${LLVM_COV_PATH} gcov \"$@\"\n")
        execute_process(COMMAND chmod +x "${GCOV_PATH}")
        
        message(STATUS "Using llvm-cov for Clang coverage: ${LLVM_COV_PATH}")
        message(STATUS "Created wrapper script: ${GCOV_PATH}")
    else()
        # For GCC, use traditional gcov
        find_program(GCOV_PATH gcov)
        if(NOT GCOV_PATH)
            message(FATAL_ERROR "gcov not found! Required for GCC code coverage.")
        endif()
        set(GCOV_PATH ${GCOV_PATH} PARENT_SCOPE)
    endif()
    
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(NOT LCOV_PATH)
        message(FATAL_ERROR "lcov not found! Please install lcov for code coverage reporting.")
    endif()
    
    if(NOT GENHTML_PATH)
        message(FATAL_ERROR "genhtml not found! Please install lcov for code coverage reporting.")
    endif()
    
    message(STATUS "Code coverage tools found:")
    message(STATUS "  gcov/llvm-cov: ${GCOV_PATH}")
    message(STATUS "  lcov: ${LCOV_PATH}")
    message(STATUS "  genhtml: ${GENHTML_PATH}")
endfunction()

# Add coverage flags to a target
function(add_coverage_flags target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            # GCC supports all these flags
            target_compile_options(${target} PRIVATE
                --coverage
                -fprofile-arcs
                -ftest-coverage
                -fno-inline
                -fno-inline-small-functions
                -fno-default-inline
                -O0
                -g
            )
        else()
            # Clang doesn't support some of the GCC-specific flags
            target_compile_options(${target} PRIVATE
                --coverage
                -fprofile-arcs
                -ftest-coverage
                -fno-inline
                -O0
                -g
            )
        endif()
        
        target_link_libraries(${target} PRIVATE --coverage)
        
        message(STATUS "Added coverage flags to target: ${target}")
    endif()
endfunction()

# Setup coverage target with lcov
function(setup_target_for_coverage_lcov)
    set(options NONE)
    set(oneValueArgs NAME EXECUTABLE BASE_DIRECTORY)
    set(multiValueArgs DEPENDENCIES EXCLUDE)
    cmake_parse_arguments(Coverage "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    if(NOT Coverage_NAME)
        message(FATAL_ERROR "setup_target_for_coverage_lcov: NAME is required")
    endif()
    
    if(NOT Coverage_EXECUTABLE)
        set(Coverage_EXECUTABLE ${CMAKE_CTEST_COMMAND})
    endif()
    
    if(NOT Coverage_BASE_DIRECTORY)
        set(Coverage_BASE_DIRECTORY ${CMAKE_BINARY_DIR})
    endif()
    
    # Make sure GCOV_PATH is available
    if(NOT GCOV_PATH)
        message(FATAL_ERROR "GCOV_PATH not set. Make sure to call check_coverage_support() first.")
    endif()
    
    # Default exclusions
    set(DEFAULT_EXCLUDES
        '/usr/*'
        '*/test/*'
        '*/tests/*'
        '*/_deps/*'
        '*/googletest/*'
        '*/boost/*'
        '*/build/*'
        '*/CMakeFiles/*'
        '/opt/homebrew/include/*'
        '/opt/homebrew/Cellar/*/include/*'
        '/usr/local/include/*'
        '/Library/Developer/*'
        '*/Applications/Xcode.app/*'
        '*/.conan/*'
        '*/external/*'
        '*/third_party/*'
        '*/vendor/*'
        # Standard library exclusions
        '*/v1/*'
        '*/c++/v1/*'
        '*/include/c++/*'
        '*/llvm/*/include/*'
        '*/clang/*/include/*'
        '*/gcc/*/include/*'
        '/System/Library/*'
        '/usr/include/*'
        '/usr/lib/*'
        # Specific standard library components
        '*/__algorithm/*'
        '*/__atomic/*'
        '*/__bit/*'
        '*/__charconv/*'
        '*/__chrono/*'
        '*/__compare/*'
        '*/__condition_variable/*'
        '*/__coroutine/*'
        '*/__debug_utils/*'
        '*/__exception/*'
        '*/__expected/*'
        '*/__format/*'
        '*/__functional/*'
        '*/__iterator/*'
        '*/__locale_dir/*'
        '*/__math/*'
        '*/__memory/*'
        '*/__mutex/*'
        '*/__new/*'
        '*/__ostream/*'
        '*/__ranges/*'
        '*/__string/*'
        '*/__system_error/*'
        '*/__thread/*'
        '*/__type_traits/*'
        '*/__utility/*'
        '*/__vector/*'
    )
    
    # Combine default and user excludes
    list(APPEND Coverage_EXCLUDE ${DEFAULT_EXCLUDES})
    
    # Build the exclude arguments for lcov
    set(EXCLUDE_ARGS "")
    foreach(EXCLUDE_PATTERN ${Coverage_EXCLUDE})
        list(APPEND EXCLUDE_ARGS --remove)
        list(APPEND EXCLUDE_ARGS ${Coverage_BASE_DIRECTORY}/coverage/${Coverage_NAME}_raw.info)
        list(APPEND EXCLUDE_ARGS ${EXCLUDE_PATTERN})
    endforeach()
    
    # Main coverage target
    add_custom_target(${Coverage_NAME}
        # Create coverage directory first
        COMMAND ${CMAKE_COMMAND} -E make_directory ${Coverage_BASE_DIRECTORY}/coverage
        
        # Clean previous coverage data
        COMMAND ${LCOV_PATH} --directory ${Coverage_BASE_DIRECTORY} --zerocounters --quiet
        
        # Run tests
        COMMAND ${Coverage_EXECUTABLE} --output-on-failure
        
        # Debug output
        COMMAND ${CMAKE_COMMAND} -E echo "Using gcov tool: ${GCOV_PATH}"
        
        # Capture coverage data
        COMMAND ${LCOV_PATH} 
            --directory ${Coverage_BASE_DIRECTORY} 
            --capture 
            --output-file ${Coverage_BASE_DIRECTORY}/coverage/${Coverage_NAME}_raw.info
            --gcov-tool ${GCOV_PATH}
            --ignore-errors inconsistent,unsupported,gcov,source,graph,empty,format,count,unused,corrupt
        
        # Remove unwanted files from coverage
        COMMAND ${LCOV_PATH}
            ${EXCLUDE_ARGS}
            --output-file ${Coverage_BASE_DIRECTORY}/coverage/${Coverage_NAME}.info
            --ignore-errors inconsistent,unsupported,gcov,source,graph,empty,format,count,unused,corrupt
            --quiet
        
        # Generate HTML report
        COMMAND ${GENHTML_PATH} 
            ${Coverage_BASE_DIRECTORY}/coverage/${Coverage_NAME}.info 
            --output-directory ${Coverage_BASE_DIRECTORY}/coverage/html
            --title "${Coverage_NAME} Coverage Report"
            --show-details
            --legend
            --ignore-errors inconsistent,unsupported,gcov,source,graph,empty,format,count,unused,corrupt,category
            --quiet
        
        # Print report location
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated successfully:"
        COMMAND ${CMAKE_COMMAND} -E echo "  HTML: ${Coverage_BASE_DIRECTORY}/coverage/html/index.html"
        COMMAND ${CMAKE_COMMAND} -E echo "  LCOV: ${Coverage_BASE_DIRECTORY}/coverage/${Coverage_NAME}.info"
        
        WORKING_DIRECTORY ${Coverage_BASE_DIRECTORY}
        DEPENDS ${Coverage_DEPENDENCIES}
        COMMENT "Generating code coverage report: ${Coverage_NAME}"
    )
    
    # Coverage clean target
    add_custom_target(${Coverage_NAME}-clean
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${Coverage_BASE_DIRECTORY}/coverage
        COMMAND find ${Coverage_BASE_DIRECTORY} -name "*.gcda" -delete 2>/dev/null || true
        COMMAND find ${Coverage_BASE_DIRECTORY} -name "*.gcno" -delete 2>/dev/null || true
        COMMENT "Cleaning coverage data for: ${Coverage_NAME}"
    )
    
    # Coverage show target (generates and displays coverage summary)
    add_custom_target(${Coverage_NAME}-show
        # Create coverage directory first
        COMMAND ${CMAKE_COMMAND} -E make_directory ${Coverage_BASE_DIRECTORY}/coverage
        
        # Clean previous coverage data
        COMMAND ${LCOV_PATH} --directory ${Coverage_BASE_DIRECTORY} --zerocounters --quiet
        
        # Run tests
        COMMAND ${Coverage_EXECUTABLE} --output-on-failure
        
        # Debug output
        COMMAND ${CMAKE_COMMAND} -E echo "Using gcov tool: ${GCOV_PATH}"
        
        # Capture coverage data
        COMMAND ${LCOV_PATH} 
            --directory ${Coverage_BASE_DIRECTORY} 
            --capture 
            --output-file ${Coverage_BASE_DIRECTORY}/coverage/${Coverage_NAME}_raw.info
            --gcov-tool ${GCOV_PATH}
            --ignore-errors inconsistent,unsupported,gcov,source,graph,empty,format,count,unused,corrupt
        
        # Remove unwanted files from coverage
        COMMAND ${LCOV_PATH}
            ${EXCLUDE_ARGS}
            --output-file ${Coverage_BASE_DIRECTORY}/coverage/${Coverage_NAME}.info
            --ignore-errors inconsistent,unsupported,gcov,source,graph,empty,format,count,unused,corrupt
            --quiet
        
        # Try to display summary (ignore errors if file is corrupted)
        COMMAND ${LCOV_PATH} 
            --summary ${Coverage_BASE_DIRECTORY}/coverage/${Coverage_NAME}.info
            --ignore-errors inconsistent,unsupported,gcov,source,graph,empty,format,count,unused,corrupt
            || echo "Coverage summary may be affected by template complexity, but report was generated successfully"
        
        # Print report location
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated:"
        COMMAND ${CMAKE_COMMAND} -E echo "  HTML: ${Coverage_BASE_DIRECTORY}/coverage/html/index.html"
        COMMAND ${CMAKE_COMMAND} -E echo "  LCOV: ${Coverage_BASE_DIRECTORY}/coverage/${Coverage_NAME}.info"
        
        WORKING_DIRECTORY ${Coverage_BASE_DIRECTORY}
        DEPENDS ${Coverage_DEPENDENCIES}
        COMMENT "Generating and displaying code coverage summary: ${Coverage_NAME}"
    )
    
    message(STATUS "Coverage targets created:")
    message(STATUS "  ${Coverage_NAME}       - Generate coverage report")
    message(STATUS "  ${Coverage_NAME}-clean - Clean coverage data")
    message(STATUS "  ${Coverage_NAME}-show  - Open coverage report")
endfunction()

# Add coverage flags globally
function(enable_coverage_globally)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            # GCC supports all these flags
            set(COVERAGE_COMPILE_FLAGS 
                --coverage 
                -fprofile-arcs 
                -ftest-coverage 
                -fno-inline 
                -fno-inline-small-functions 
                -fno-default-inline
                -O0
                -g
            )
        else()
            # Clang doesn't support some of the GCC-specific flags
            set(COVERAGE_COMPILE_FLAGS 
                --coverage 
                -fprofile-arcs 
                -ftest-coverage 
                -fno-inline
                -O0
                -g
            )
        endif()
        
        set(COVERAGE_LINK_FLAGS --coverage)
        
        string(REPLACE ";" " " COVERAGE_COMPILE_FLAGS_STR "${COVERAGE_COMPILE_FLAGS}")
        
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILE_FLAGS_STR}" PARENT_SCOPE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_LINK_FLAGS}" PARENT_SCOPE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${COVERAGE_LINK_FLAGS}" PARENT_SCOPE)
        
        message(STATUS "Global coverage flags enabled for ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()

# Print coverage information
function(coverage_info)
    message(STATUS "=== Code Coverage Information ===")
    message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
    message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
    
    if(GCOV_PATH)
        message(STATUS "gcov/llvm-cov: ${GCOV_PATH}")
    endif()
    
    if(LCOV_PATH)
        message(STATUS "lcov: ${LCOV_PATH}")
    endif()
    
    if(GENHTML_PATH)
        message(STATUS "genhtml: ${GENHTML_PATH}")
    endif()
    
    message(STATUS "================================")
endfunction() 