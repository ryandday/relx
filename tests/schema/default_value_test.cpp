#include <gtest/gtest.h>
#include <relx/schema/column.hpp>
#include <relx/schema/default_value.hpp>
#include <relx/schema/table.hpp>
#include <string>

using namespace relx::schema;

// Test table with default value columns
struct Product {
    static constexpr auto table_name = "products";
    
    column<"id", int> id;
    column<"product_name", std::string> product_name;
    column<"price", double, DefaultValue<0.0>> price;
    column<"stock", int, DefaultValue<10>> stock;
    column<"active", bool, DefaultValue<true>> active;
};

TEST(DefaultValueTest, BasicDefaultValues) {
    // Test integer default value
    column<"count", int, DefaultValue<5>> count_col;
    EXPECT_TRUE(count_col.has_default);
    EXPECT_EQ(count_col.sql_definition(), "count INTEGER NOT NULL DEFAULT 5");
    auto count_default = count_col.get_default_value();
    ASSERT_TRUE(count_default.has_value());
    EXPECT_EQ(*count_default, 5);
    
    // Test double default value
    column<"price", double, DefaultValue<19.99>> price_col;
    EXPECT_TRUE(price_col.has_default);
    EXPECT_EQ(price_col.sql_definition(), "price REAL NOT NULL DEFAULT 19.990000");
    auto price_default = price_col.get_default_value();
    ASSERT_TRUE(price_default.has_value());
    EXPECT_DOUBLE_EQ(*price_default, 19.99);
    
    // Test bool default value
    column<"is_active", bool, DefaultValue<true>> is_active_col;
    EXPECT_TRUE(is_active_col.has_default);
    EXPECT_EQ(is_active_col.sql_definition(), "is_active INTEGER NOT NULL DEFAULT 1");
    auto is_active_default = is_active_col.get_default_value();
    ASSERT_TRUE(is_active_default.has_value());
    EXPECT_EQ(*is_active_default, true);
}

TEST(DefaultValueTest, NullableColumnsWithDefaults) {
    // Test nullable column with default value
    column<"count", std::optional<int>, DefaultValue<42>> count_col;
    EXPECT_TRUE(count_col.has_default);
    EXPECT_TRUE(count_col.nullable);
    EXPECT_EQ(count_col.sql_definition(), "count INTEGER DEFAULT 42");
    auto count_default = count_col.get_default_value();
    ASSERT_TRUE(count_default.has_value());
    EXPECT_EQ(**count_default, 42);
    
    // Test nullable column with NULL default
    column<"notes", std::optional<std::string>, null_default> notes_col;
    EXPECT_TRUE(notes_col.has_default);
    EXPECT_TRUE(notes_col.nullable);
    EXPECT_EQ(notes_col.sql_definition(), "notes TEXT DEFAULT NULL");
}

TEST(DefaultValueTest, TableWithDefaults) {
    Product p;
    
    // Generate CREATE TABLE SQL with default values
    std::string create_sql = create_table(p);
    
    // Validate SQL contains default values
    EXPECT_TRUE(create_sql.find("price REAL NOT NULL DEFAULT 0.000000") != std::string::npos);
    EXPECT_TRUE(create_sql.find("stock INTEGER NOT NULL DEFAULT 10") != std::string::npos);
    EXPECT_TRUE(create_sql.find("active INTEGER NOT NULL DEFAULT 1") != std::string::npos);
} 
