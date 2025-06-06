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

# Add main test executable
add_executable(relx_tests
    main_test.cpp
    # Schema tests
    schema/fixed_string_test.cpp
    schema/column_test.cpp
    schema/table_test.cpp
    schema/primary_key_test.cpp
    schema/foreign_key_test.cpp
    schema/index_test.cpp
    schema/complex_schema_test.cpp
    schema/concepts_test.cpp
    schema/custom_column_types_test.cpp
    schema/default_value_test.cpp
    schema/check_constraint_test.cpp
    schema/unique_constraint_test.cpp
    schema/optional_column_test.cpp
    schema/composite_foreign_key_test.cpp
    schema/constraint_alias_test.cpp
    # Query tests
    query/query_test.cpp
    query/basic_select_test.cpp
    query/join_test.cpp
    query/condition_test.cpp
    query/aggregate_test.cpp
    query/case_expression_test.cpp
    query/string_function_test.cpp
    query/arithmetic_test.cpp
    query/date_test.cpp
    query/date_multi_clock_test.cpp
    query/update_query_test.cpp
    query/delete_query_test.cpp
    query/insert_query_test.cpp
    query/select_all_test.cpp
    query/distinct_select_test.cpp
    query/edge_case_test.cpp
    query/data_type_test.cpp
    column_like_test.cpp
    # Result processing tests
    result_test.cpp
    advanced_query_test.cpp
    type_safety_test.cpp
    # Connection tests
    connection/postgresql_connection_test.cpp
    connection/postgresql_api_test.cpp
    connection/postgresql_typed_params_test.cpp
    connection/postgresql_binary_test.cpp
    connection/postgresql_statement_test.cpp
    connection/postgresql_connection_pool_test.cpp
    connection/dto_mapping_test.cpp
    connection/postgresql_async_wrapper_test.cpp
    # PostgreSQL Integration tests
    postgres_integration/basic_integration_test.cpp
    postgres_integration/schema_integration_test.cpp
    postgres_integration/join_integration_test.cpp
    postgres_integration/dml_integration_test.cpp
    # postgres_integration/connection_pool_integration_test.cpp
    postgres_integration/query_integration_test.cpp
    postgres_integration/dto_mapping_integration_test.cpp
    postgres_integration/async_dto_mapping_test.cpp
    # Utility tests
    utility/throw_if_failed_test.cpp
)

# Link against both gtest and gtest_main libraries
target_link_libraries(relx_tests PRIVATE 
    relx_postgresql 
    gtest
    gtest_main
)

# Register test
add_test(NAME relx_tests COMMAND relx_tests)

# Add static_assert tests executable
add_executable(static_assert_tests static_assert_tests.cpp)
target_link_libraries(static_assert_tests PRIVATE relx_postgresql) 