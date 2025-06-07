# Migration library
add_library(relx_migrations
    command_line_tools.cpp
    diff.cpp
)

target_include_directories(relx_migrations 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/relx
)

target_link_libraries(relx_migrations 
    PUBLIC 
        relx  # Link to the main header-only library
    PRIVATE
        Boost::system
)

target_compile_features(relx_migrations PUBLIC cxx_std_23)

# Alias for easier usage
add_library(relx::migrations ALIAS relx_migrations) 