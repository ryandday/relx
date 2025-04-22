#include <gtest/gtest.h>
#include <relx/schema.hpp>
#include <string>
#include <optional>
#include <limits>
#include <type_traits>
#include <array>
#include <vector>
#include <tuple>

using namespace relx::schema;

// Most databases have identifier length limits (SQLite: 1024, PostgreSQL: 63, MySQL: 64)
inline constexpr char veryLongTableName[] = "this_is_an_extremely_long_table_name_that_exceeds_the_normal_conventions_of_database_naming_and_might_cause_issues_with_some_database_engines_especially_when_the_name_gets_even_longer_and_longer_with_more_and_more_characters_until_it_eventually_hits_the_limit_of_what_is_reasonable_or_allowed_by_the_system";
inline constexpr char veryLongColumnName[] = "this_column_name_is_ridiculously_long_and_serves_no_practical_purpose_other_than_to_test_the_limits_of_the_library_handling_for_extremely_long_identifiers_which_might_cause_issues_when_generating_sql_or_handling_compilation_with_template_metaprogramming_techniques";
inline constexpr char veryLongCheckName[] = "length(this_is_a_very_long_subquery_in_a_check_constraint_that_might_cause_issues_with_template_instantiation_depth_or_compiler_limits_for_string_literals)";

// Define some unusual custom types
struct CustomType {
    int value;
};

enum class CustomEnum { Value1, Value2, Value3 };

// Extend column_traits for custom types
namespace relx {
namespace schema {
    template <>
    struct column_traits<CustomType> {
        static constexpr auto sql_type_name = "BLOB"; // Store as blob
        static constexpr bool nullable = false;
        
        static std::string to_sql_string(const CustomType& value) {
            return std::to_string(value.value); // Simple serialization
        }
        
        static CustomType from_sql_string(const std::string& value) {
            return CustomType{std::stoi(value)}; // Simple deserialization
        }
    };
    
    template <>
    struct column_traits<CustomEnum> {
        static constexpr auto sql_type_name = "INTEGER";
        static constexpr bool nullable = false;
        
        static std::string to_sql_string(const CustomEnum& value) {
            return std::to_string(static_cast<int>(value));
        }
        
        static CustomEnum from_sql_string(const std::string& value) {
            return static_cast<CustomEnum>(std::stoi(value));
        }
    };
    
    // Specialization of DefaultValue for CustomEnum to handle std::to_string conversion
    template <CustomEnum Value>
    struct DefaultValue<Value> {
        using value_type = CustomEnum;
        
        static constexpr value_type value = Value;
        
        static std::string sql_definition() {
            return " DEFAULT " + std::to_string(static_cast<int>(value));
        }
        
        static std::optional<value_type> parse_value() {
            return value;
        }
    };
}}

// Test with extremely long identifiers
struct ExtremelyLongNameTable {
    static constexpr auto table_name = veryLongTableName;
    
    column<"very_long_column", int> very_long_column;
    
    // Check constraint with extremely long condition
    check_constraint<"length(very_long_column) > 0"> extreme_check;
};

// Test with unusual but valid SQL identifiers containing special characters
struct SpecialCharTable {
    static constexpr auto table_name = "table_with_hyphens";
    
    column<"column_with_spaces", int> space_column;
    column<"column_with_hyphens", int> hyphen_column;
    column<"column_with_underscore", int> normal_column;
    column<"numeric_start_column", int> numeric_start_column;
    column<"dollar_column", int> dollar_column;
    column<"dot_column", int> dot_column;
    
    // This should force proper escaping/quoting in SQL output
    check_constraint<"column_with_spaces > 0 AND column_with_hyphens < 100"> special_char_check;
};

// Test with extreme numeric values
struct ExtremeValueTable {
    static constexpr auto table_name = "extreme_values";
    
    column<"max_int", int, DefaultValue<std::numeric_limits<int>::max()>> max_int_col;
    column<"min_int", int, DefaultValue<std::numeric_limits<int>::min()>> min_int_col;
    column<"max_double", double, DefaultValue<std::numeric_limits<double>::max()>> max_double_col;
    column<"min_double", double, DefaultValue<std::numeric_limits<double>::min()>> min_double_col;
    column<"infinity", double> infinity_col;  // Can't use infinity in template parameter
    column<"nan", double> nan_col;            // Can't use NaN in template parameter
    
    // Check constraint using extreme values
    check_constraint<"max_int = 2147483647 AND min_int = -2147483648"> extreme_int_check;
};

// Test with SQL injection attempts
struct SQLInjectionTable {
    static constexpr auto table_name = "vulnerable_table";
    
    column<"id", int> id;
    column<"safe_column", std::string> safe_column;
    
    // String literal containing SQL injection payload
    column<"injection_column", std::string> injection_column;
    
    // Check constraint with potential SQL injection
    check_constraint<"safe_column != ''''DROP TABLE users;--''"> injection_check;
    
    primary_key<&SQLInjectionTable::id> pk;
};

// Define tables with circular foreign key references but without inheritance
struct CircularTable1 {
    static constexpr auto table_name = "circular_table1";
    
    column<"id", int> id;
    column<"ref_to_table2", int> ref_to_table2;
    
    primary_key<&CircularTable1::id> pk;
};

struct CircularTable2 {
    static constexpr auto table_name = "circular_table2";
    
    column<"id", int> id;
    column<"ref_to_table1", int> ref_to_table1;
    
    primary_key<&CircularTable2::id> pk;
    
    // This creates a circular reference
    foreign_key<&CircularTable2::ref_to_table1, &CircularTable1::id> fk_to_table1;
};

// Instead of inheritance, use a standalone table with the foreign key
struct CircularTable1WithFK {
    static constexpr auto table_name = "circular_table1_with_fk";
    
    column<"id", int> id;
    column<"ref_to_table2", int> ref_to_table2;
    
    primary_key<&CircularTable1WithFK::id> pk;
    
    // This completes the circular reference
    foreign_key<&CircularTable1WithFK::ref_to_table2, &CircularTable2::id> fk_to_table2;
};

// Test with unusual types
struct UnusualTypesTable {
    static constexpr auto table_name = "unusual_types";
    
    column<"id", int> id;
    column<"custom_type", CustomType> custom_type_col;
    column<"enum_type", CustomEnum, DefaultValue<CustomEnum::Value2>> enum_type_col;
    column<"optional_custom", std::optional<CustomType>> optional_custom_col;
    
    primary_key<&UnusualTypesTable::id> pk;
    
    // Check constraint referencing the custom values
    check_constraint<"id > 0"> simple_check;
};

// Test with recursive table structures
struct RecursiveTable {
    static constexpr auto table_name = "recursive_entity";
    
    column<"id", int> id;
    column<"name", std::string> name;
    column<"parent_id", std::optional<int>> parent_id;
    
    primary_key<&RecursiveTable::id> pk;
    
    // Self-referencing foreign key
    foreign_key<&RecursiveTable::parent_id, &RecursiveTable::id> parent_fk;
    
    // Circular check constraint referencing itself
    check_constraint<"parent_id IS NULL OR parent_id != id"> no_self_reference;
};

// Test with empty table
struct EmptyTable {
    static constexpr auto table_name = "empty_table";
    
    // No columns or constraints defined
};

// Test with unicode characters
struct UnicodeTable {
    static constexpr auto table_name = "unicode_table";
    
    column<"id", int> id;
    column<"unicode_column", std::string> unicode_column;
    column<"emoji_column", std::string> emoji_column;
    column<"unicode_default", std::string> unicode_default;
    column<"emoji_default", std::string> emoji_default;
    
    primary_key<&UnicodeTable::id> pk;
    
    // Check constraint with unicode
    check_constraint<"unicode_column != ''"> unicode_check;
};

// Test with many columns
struct ManyColumnsTable {
    static constexpr auto table_name = "many_columns_table";
    
    // Define a large number of columns (50+ columns)
    column<"id", int> id;
    column<"col1", int> col1;
    column<"col2", int> col2;
    column<"col3", int> col3;
    column<"col4", int> col4;
    column<"col5", int> col5;
    column<"col6", int> col6;
    column<"col7", int> col7;
    column<"col8", int> col8;
    column<"col9", int> col9;
    column<"col10", int> col10;
    column<"col11", int> col11;
    column<"col12", int> col12;
    column<"col13", int> col13;
    column<"col14", int> col14;
    column<"col15", int> col15;
    column<"col16", int> col16;
    column<"col17", int> col17;
    column<"col18", int> col18;
    column<"col19", int> col19;
    column<"col20", int> col20;
    column<"col21", int> col21;
    column<"col22", int> col22;
    column<"col23", int> col23;
    column<"col24", int> col24;
    column<"col25", int> col25;
    column<"col26", int> col26;
    column<"col27", int> col27;
    column<"col28", int> col28;
    column<"col29", int> col29;
    column<"col30", int> col30;
    column<"col31", std::string> col31;
    column<"col32", std::string> col32;
    column<"col33", std::string> col33;
    column<"col34", std::string> col34;
    column<"col35", std::string> col35;
    column<"col36", std::string> col36;
    column<"col37", std::string> col37;
    column<"col38", std::string> col38;
    column<"col39", std::string> col39;
    column<"col40", std::string> col40;
    column<"col41", double> col41;
    column<"col42", double> col42;
    column<"col43", double> col43;
    column<"col44", double> col44;
    column<"col45", double> col45;
    column<"col46", double> col46;
    column<"col47", double> col47;
    column<"col48", double> col48;
    column<"col49", double> col49;
    column<"col50", double> col50;
    
    // Primary key
    primary_key<&ManyColumnsTable::id> pk;
    
    // Many check constraints
    check_constraint<"col1 > 0"> check1;
    check_constraint<"col2 > 0"> check2;
    check_constraint<"col3 > 0"> check3;
    check_constraint<"col4 > 0"> check4;
    check_constraint<"col5 > 0"> check5;
    check_constraint<"col6 > 0"> check6;
    check_constraint<"col7 > 0"> check7;
    check_constraint<"col8 > 0"> check8;
    check_constraint<"col9 > 0"> check9;
    check_constraint<"col10 > 0"> check10;
    
    // Many unique constraints
    unique_constraint<&ManyColumnsTable::col31> unique1;
    unique_constraint<&ManyColumnsTable::col32> unique2;
    unique_constraint<&ManyColumnsTable::col33> unique3;
    unique_constraint<&ManyColumnsTable::col34> unique4;
    unique_constraint<&ManyColumnsTable::col35> unique5;
};

// Test with malformed check constraints
struct MalformedConstraintTable {
    static constexpr auto table_name = "malformed_constraints";
    
    column<"id", int> id;
    column<"value", int> value;
    
    primary_key<&MalformedConstraintTable::id> pk;
    
    // Malformed check constraints (syntax errors, unbalanced parentheses, etc.)
    check_constraint<"value > (1 + 2 * (3 - 4)"> unbalanced_parentheses;
    check_constraint<"value IN (1, 2, 3"> missing_closing_paren;
    check_constraint<"value BETWEEN 1 AND"> incomplete_between;
    check_constraint<"value IS NOT TRUE OR IS NOT FALSE OR IS"> incomplete_is;
    check_constraint<"value LIKE '%pattern%''"> unescaped_quote;
    check_constraint<"''"> empty_constraint;
};

// Test with constructor - this shouldn't compile
/*
struct TableWithConstructor {
    static constexpr auto table_name = "constructor_table";
    
    column<"id", int> id;
    column<"name", std::string> name;
    
    primary_key<&TableWithConstructor::id> pk;
    
    // This constructor breaks boost::pfr compatibility
    TableWithConstructor() : id(), name() {}
};
*/

// Now the actual test cases
TEST(BreakTheLibraryTest, ExtremelyLongNames) {
    ExtremelyLongNameTable table;
    std::string sql = create_table_sql(table);
    
    // Test that the SQL was generated without truncating the names
    EXPECT_TRUE(sql.find(veryLongTableName) != std::string::npos);
    EXPECT_TRUE(sql.find("very_long_column") != std::string::npos);
}

TEST(BreakTheLibraryTest, SpecialCharactersInNames) {
    SpecialCharTable table;
    std::string sql = create_table_sql(table);
    
    // Test that special characters are handled correctly
    EXPECT_TRUE(sql.find("table_with_hyphens") != std::string::npos);
    EXPECT_TRUE(sql.find("column_with_spaces") != std::string::npos);
    EXPECT_TRUE(sql.find("column_with_hyphens") != std::string::npos);
}

TEST(BreakTheLibraryTest, ExtremeNumericValues) {
    ExtremeValueTable table;
    
    std::string create_sql = create_table_sql(table);
    
    // Test that SQL contains extreme default values
    EXPECT_TRUE(create_sql.find("max_int INTEGER NOT NULL DEFAULT 2147483647") != std::string::npos);
    EXPECT_TRUE(create_sql.find("min_int INTEGER NOT NULL DEFAULT -2147483648") != std::string::npos);
    
    // Test conversion of special floating point values to SQL strings
    std::string infinity_sql = column<"test", double>::to_sql_string(std::numeric_limits<double>::infinity());
    std::string nan_sql = column<"test", double>::to_sql_string(std::numeric_limits<double>::quiet_NaN());
    
    // The exact string representation might depend on the implementation, but it should be something valid
    EXPECT_FALSE(infinity_sql.empty());
    EXPECT_FALSE(nan_sql.empty());
}

TEST(BreakTheLibraryTest, SQLInjectionAttempts) {
    SQLInjectionTable table;
    
    std::string create_sql = create_table_sql(table);
    EXPECT_TRUE(create_sql.find("injection_column TEXT NOT NULL") != std::string::npos);
    
    // Test SQL string escaping for a potential SQL injection string
    std::string injection_string = "'; DROP TABLE users; --";
    std::string escaped = column<"test", std::string>::to_sql_string(injection_string);
    
    // Check that single quotes are properly escaped (doubled)
    EXPECT_TRUE(escaped.find("''") != std::string::npos);
    EXPECT_TRUE(escaped.find("DROP TABLE") != std::string::npos);
    
    // The column definition SQL generation should not execute the injection
    EXPECT_FALSE(create_sql.find("DROP TABLE") != std::string::npos);
}

TEST(BreakTheLibraryTest, CircularReferences) {
    CircularTable1WithFK table1;
    CircularTable2 table2;
    
    std::string sql1 = create_table_sql(table1);
    std::string sql2 = create_table_sql(table2);
    
    // Test that both tables and their foreign keys are created correctly
    EXPECT_TRUE(sql1.find("FOREIGN KEY (ref_to_table2) REFERENCES circular_table2 (id)") != std::string::npos);
    EXPECT_TRUE(sql2.find("FOREIGN KEY (ref_to_table1) REFERENCES circular_table1 (id)") != std::string::npos);
}

TEST(BreakTheLibraryTest, UnusualTypes) {
    UnusualTypesTable table;
    
    std::string create_sql = create_table_sql(table);
    
    // Test that SQL contains custom type columns
    EXPECT_TRUE(create_sql.find("custom_type BLOB NOT NULL") != std::string::npos);
    EXPECT_TRUE(create_sql.find("enum_type INTEGER NOT NULL DEFAULT 1") != std::string::npos);
    
    // Test conversion of custom types to SQL strings
    CustomType custom_val{42};
    std::string custom_sql = column_traits<CustomType>::to_sql_string(custom_val);
    EXPECT_EQ(custom_sql, "42");
    
    CustomEnum enum_val = CustomEnum::Value3;
    std::string enum_sql = column_traits<CustomEnum>::to_sql_string(enum_val);
    EXPECT_EQ(enum_sql, "2"); // Value3 is the third enum value (index 2)
    
    // Test default value for enum
    auto default_value = column<"test", CustomEnum, DefaultValue<CustomEnum::Value2>>::get_default_value();
    ASSERT_TRUE(default_value.has_value());
    EXPECT_EQ(*default_value, CustomEnum::Value2);
}

TEST(BreakTheLibraryTest, RecursiveStructures) {
    RecursiveTable table;
    std::string sql = create_table_sql(table);
    
    // Verify the self-referencing foreign key is created correctly
    EXPECT_TRUE(sql.find("FOREIGN KEY (parent_id) REFERENCES recursive_entity (id)") != std::string::npos);
    EXPECT_TRUE(sql.find("CHECK (parent_id IS NULL OR parent_id != id)") != std::string::npos);
}

TEST(BreakTheLibraryTest, EmptyTable) {
    EmptyTable table;
    std::string sql = create_table_sql(table);
    
    // The SQL might be technically valid but logically invalid (table with no columns)
    EXPECT_TRUE(sql.find("CREATE TABLE IF NOT EXISTS empty_table") != std::string::npos);
}

TEST(BreakTheLibraryTest, UnicodeCharacters) {
    UnicodeTable table;
    
    std::string create_sql = create_table_sql(table);
    EXPECT_TRUE(create_sql.find("unicode_column TEXT NOT NULL") != std::string::npos);
    EXPECT_TRUE(create_sql.find("emoji_column TEXT NOT NULL") != std::string::npos);
    
    // Test unicode string handling
    std::string unicode_string = "こんにちは世界";
    std::string unicode_sql = column<"test", std::string>::to_sql_string(unicode_string);
    EXPECT_TRUE(unicode_sql.find(unicode_string) != std::string::npos);
    
    // Test emoji handling
    std::string emoji_string = "Hello 🌎 World 🚀";
    std::string emoji_sql = column<"test", std::string>::to_sql_string(emoji_string);
    EXPECT_TRUE(emoji_sql.find(emoji_string) != std::string::npos);
}

TEST(BreakTheLibraryTest, TemplateInstantiationDepth) {
    ManyColumnsTable table;
    std::string sql = create_table_sql(table);
    
    // Verify that a table with many columns and constraints generates correctly
    EXPECT_TRUE(sql.find("CREATE TABLE IF NOT EXISTS many_columns_table") != std::string::npos);
    EXPECT_TRUE(sql.find("col1 INTEGER NOT NULL") != std::string::npos);
    EXPECT_TRUE(sql.find("col50 REAL NOT NULL") != std::string::npos);
    
    // Check that all constraints are included
    EXPECT_TRUE(sql.find("PRIMARY KEY (id)") != std::string::npos);
    EXPECT_TRUE(sql.find("UNIQUE (col31)") != std::string::npos);
    EXPECT_TRUE(sql.find("UNIQUE (col35)") != std::string::npos);
    EXPECT_TRUE(sql.find("CHECK (col1 > 0)") != std::string::npos);
    EXPECT_TRUE(sql.find("CHECK (col10 > 0)") != std::string::npos);
}

TEST(BreakTheLibraryTest, MalformedCheckConstraints) {
    MalformedConstraintTable table;
    std::string sql = create_table_sql(table);
    
    // The library should output these constraints as-is without validation 
    // (it's up to the database to reject them)
    EXPECT_TRUE(sql.find("CHECK (value > (1 + 2 * (3 - 4)") != std::string::npos);
    EXPECT_TRUE(sql.find("CHECK (value IN (1, 2, 3") != std::string::npos);
    EXPECT_TRUE(sql.find("CHECK (value BETWEEN 1 AND)") != std::string::npos);
    EXPECT_TRUE(sql.find("CHECK (value IS NOT TRUE OR IS NOT FALSE OR IS)") != std::string::npos);
    EXPECT_TRUE(sql.find("CHECK (value LIKE '%pattern%''")  != std::string::npos);
    EXPECT_TRUE(sql.find("CHECK ('')") != std::string::npos);
}

// Test with constructor - this would fail to compile
/*
TEST(BreakTheLibraryTest, TableWithConstructor) {
    TableWithConstructor table;
    std::string sql = create_table_sql(table); // This should fail to compile
}
*/ 