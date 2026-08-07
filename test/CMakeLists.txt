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
    schema/complex_schema_test.cpp
    schema/custom_column_types_test.cpp
    schema/default_value_test.cpp
    schema/check_constraint_test.cpp
    schema/optional_column_test.cpp
    schema/column_like_test.cpp
    schema/timezone_parsing_test.cpp
    schema/annotated_table_test.cpp
    schema/enum_column_test.cpp
    schema/jsonb_test.cpp
    schema/traits_strictness_test.cpp
    schema/uuid_traits_test.cpp
    # Migration tests
    migrations/test_migrations.cpp
    migrations/test_command_line_tools.cpp
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
    query/struct_writes_test.cpp
    query/table_alias_test.cpp
    query/example_filter_test.cpp
    query/select_all_test.cpp
    query/subquery_test.cpp
    query/bind_param_test.cpp
    query/static_sql_test.cpp
    query/web_core_gaps_test.cpp
    query/distinct_select_test.cpp
    query/quoting_and_guards_test.cpp
    query/edge_case_test.cpp
    query/data_type_test.cpp
    query/advanced_query_test.cpp
    query/row_type_test.cpp
    query/nested_row_test.cpp
    # Result processing tests
    result/result_test.cpp
    result/lazy_parsing_test.cpp
    result/result_strictness_test.cpp
    result/pipe_format_roundtrip_test.cpp
    # Type safety tests
    types/type_safety_test.cpp
    # Connection tests
    connection/transaction_guard_test.cpp
    connection/postgresql_connection_test.cpp
    connection/postgresql_api_test.cpp
    connection/postgresql_typed_params_test.cpp
    connection/postgresql_binary_test.cpp
    connection/postgresql_statement_test.cpp
    connection/postgresql_connection_pool_test.cpp
    connection/postgresql_placeholder_test.cpp
    connection/dto_mapping_test.cpp
    connection/postgresql_async_wrapper_test.cpp
    connection/postgresql_streaming_test.cpp
    connection/postgresql_async_streaming_test.cpp
    # Seeded property fuzzing for relx-owned parsers
    fuzz/parser_fuzz_test.cpp
    # PostgreSQL Integration tests
    postgres_integration/postgresql_integration_test.cpp
    postgres_integration/basic_integration_test.cpp
    postgres_integration/schema_integration_test.cpp
    postgres_integration/join_integration_test.cpp
    postgres_integration/dml_integration_test.cpp
    postgres_integration/query_integration_test.cpp
    postgres_integration/dto_mapping_integration_test.cpp
    postgres_integration/async_dto_mapping_test.cpp
    postgres_integration/date_time_integration_test.cpp
    postgres_integration/pool_recovery_test.cpp
    postgres_integration/annotated_table_integration_test.cpp
    postgres_integration/struct_writes_integration_test.cpp
    postgres_integration/nested_row_integration_test.cpp
    postgres_integration/schema_verify_integration_test.cpp
    postgres_integration/jsonb_integration_test.cpp
    postgres_integration/prepared_cache_integration_test.cpp
    postgres_integration/table_alias_integration_test.cpp
    # Utility tests
    utility/throw_if_failed_test.cpp
    utility/refl_types_test.cpp
    utility/debug_dump_test.cpp
    utility/sql_literal_test.cpp
)

# Link against both gtest and gtest_main libraries
target_link_libraries(relx_tests PRIVATE 
    relx_postgresql 
    relx_migrations
    gtest
    gtest_main
)

# Register test
add_test(NAME relx_tests COMMAND relx_tests)

# Compile-failure tests: each source must FAIL to compile with the diagnostic it
# declares (a "// expect-error: <regex>" line in the file), checked with
# -fsyntax-only so no artifacts are produced. Matching the intended diagnostic -
# rather than accepting any failure - prevents false passes from unrelated breakage
# (a missing include path once made every case "fail" on libpq-fe.h and score green).
# control_must_compile.cpp guards against the suite passing for the wrong reason.
find_package(PostgreSQL REQUIRED)
set(RELX_COMPILE_FAIL_SOURCES
    compile_fail/composite_pk_unknown_column.cpp
    compile_fail/duplicate_composite_pk.cpp
    compile_fail/composite_pk_conflicts_with_member_pk.cpp
    compile_fail/composite_fk_targets_non_key.cpp
    compile_fail/select_missing_row_field.cpp
    compile_fail/string_default_sql_expression.cpp
    compile_fail/self_join_without_alias.cpp
    compile_fail/scalar_subquery_in_select_list.cpp
    compile_fail/values_from_missing_field.cpp
    compile_fail/upsert_without_pk.cpp
    compile_fail/upsert_pk_not_inserted.cpp
    compile_fail/set_from_non_optional_field.cpp
    compile_fail/set_from_unknown_column.cpp
    compile_fail/nested_row_right_join.cpp
    compile_fail/sql_literal_gap.cpp
    compile_fail/sql_literal_count_mismatch.cpp
    compile_fail/type_mismatch_comparison.cpp
    compile_fail/arithmetic_on_string.cpp
    compile_fail/arithmetic_on_bool.cpp
    compile_fail/string_view_dto_field.cpp
    compile_fail/char_dto_field.cpp
    compile_fail/select_zero_columns.cpp
    compile_fail/update_set_duplicate_column.cpp
    compile_fail/insert_values_arity_mismatch.cpp
    compile_fail/table_annotation_on_member.cpp
    compile_fail/column_annotation_on_struct.cpp
)
foreach(compile_fail_src ${RELX_COMPILE_FAIL_SOURCES})
  get_filename_component(compile_fail_name ${compile_fail_src} NAME_WE)
  add_test(NAME compile_fail_${compile_fail_name}
           COMMAND ${CMAKE_CXX_COMPILER} -std=gnu++26 -freflection -fsyntax-only
                   -I${CMAKE_SOURCE_DIR}/include
                   -isystem ${PostgreSQL_INCLUDE_DIRS}
                   ${CMAKE_CURRENT_SOURCE_DIR}/${compile_fail_src})
  file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/${compile_fail_src} expect_error_line
       REGEX "// expect-error: ")
  if(expect_error_line)
    string(REGEX REPLACE ".*// expect-error: " "" expect_error "${expect_error_line}")
    # PASS_REGULAR_EXPRESSION: the test passes iff the compiler output contains the
    # declared diagnostic (exit code is irrelevant - success without the diagnostic
    # and failure for a different reason both fail the test)
    set_tests_properties(compile_fail_${compile_fail_name}
                         PROPERTIES PASS_REGULAR_EXPRESSION "${expect_error}")
  else()
    message(FATAL_ERROR "${compile_fail_src} lacks a '// expect-error:' annotation")
  endif()
endforeach()

add_test(NAME compile_fail_control_must_compile
         COMMAND ${CMAKE_CXX_COMPILER} -std=gnu++26 -freflection -fsyntax-only
                 -I${CMAKE_SOURCE_DIR}/include
                 -isystem ${PostgreSQL_INCLUDE_DIRS}
                 ${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/control_must_compile.cpp)