# PostgreSQL client library
find_package(PostgreSQL REQUIRED)

add_library(relx_postgresql
    connection/postgresql_connection.cpp
    connection/postgresql_connection_pool.cpp
    connection/postgresql_errors.cpp
    connection/postgresql_statement.cpp
    connection/pgsql_async_wrapper.cpp
    connection/postgresql_async_connection.cpp
    connection/postgresql_streaming_source.cpp
    connection/postgresql_async_streaming_source.cpp
    connection/sql_utils.cpp
)

target_include_directories(relx_postgresql 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/relx
)

target_link_libraries(relx_postgresql 
    PUBLIC 
        relx  # Link to the main header-only library
        PostgreSQL::PostgreSQL
    PRIVATE
        Boost::system
        Boost::thread
)

# Alias for easier usage
add_library(relx::postgresql ALIAS relx_postgresql) 