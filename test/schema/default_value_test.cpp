#include <gtest/gtest.h>
#include <relx/schema/column.hpp>
#include <relx/schema/table.hpp>
#include <string>

using namespace relx::schema;

// Test table with default value columns
struct Product {
    static constexpr auto table_name = "products";
    
    column<Product, "id", int> id;
    column<Product, "product_name", std::string> product_name;
    column<Product, "price", double, default_value<0.0>> price;
    column<Product, "stock", int, default_value<10>> stock;
    column<Product, "active", bool, default_value<true>> active;
    column<Product, "status", std::string, string_default<"active">> status;
};

TEST(DefaultValueTest, BasicDefaultValues) {
    // Test integer default value
    column<Product, "count", int, default_value<5>> count_col;
    EXPECT_EQ(count_col.sql_definition(), "count INTEGER NOT NULL DEFAULT 5");
    auto count_default = count_col.get_default_value();
    ASSERT_TRUE(count_default.has_value());
    EXPECT_EQ(*count_default, 5);
    
    // Test double default value
    column<Product, "price", double, default_value<19.99>> price_col;
    EXPECT_TRUE(price_col.sql_definition().find("DEFAULT 19.99") != std::string::npos);
    auto price_default = price_col.get_default_value();
    ASSERT_TRUE(price_default.has_value());
    EXPECT_DOUBLE_EQ(*price_default, 19.99);
    
    // Test bool default value
    column<Product, "is_active", bool, default_value<true>> is_active_col;
    EXPECT_EQ(is_active_col.sql_definition(), "is_active BOOLEAN NOT NULL DEFAULT true");
    auto is_active_default = is_active_col.get_default_value();
    ASSERT_TRUE(is_active_default.has_value());
    EXPECT_EQ(*is_active_default, true);
    
    // Test string default value
    column<Product, "name", std::string, string_default<"default_name">> name_col;
    EXPECT_EQ(name_col.sql_definition(), "name TEXT NOT NULL DEFAULT 'default_name'");
    auto name_default = name_col.get_default_value();
    ASSERT_TRUE(name_default.has_value());
    EXPECT_EQ(*name_default, "default_name");
    
    // Test SQL literal default value
    column<Product, "created_at", std::string, string_default<"CURRENT_TIMESTAMP", true>> created_at_col;
    EXPECT_EQ(created_at_col.sql_definition(), "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP");
}

TEST(DefaultValueTest, NullableColumnsWithDefaults) {
    // Test nullable column with default value
    column<Product, "count", std::optional<int>, default_value<42>> count_col;
    EXPECT_TRUE(count_col.nullable);
    EXPECT_EQ(count_col.sql_definition(), "count INTEGER DEFAULT 42");
    auto count_default = count_col.get_default_value();
    ASSERT_TRUE(count_default.has_value());
    EXPECT_EQ(**count_default, 42);
    
    // Test nullable column with NULL default
    column<Product, "notes", std::optional<std::string>, null_default> notes_col;
    EXPECT_TRUE(notes_col.nullable);
    EXPECT_EQ(notes_col.sql_definition(), "notes TEXT DEFAULT NULL");
    
    // Test nullable column with string default
    column<Product, "status", std::optional<std::string>, string_default<"pending">> status_col;
    EXPECT_TRUE(status_col.nullable);
    EXPECT_EQ(status_col.sql_definition(), "status TEXT DEFAULT 'pending'");
}

TEST(DefaultValueTest, TableWithDefaults) {
    Product p;
    
    // Generate CREATE TABLE SQL with default values
    std::string create_sql = create_table(p).to_sql();
    
    // Validate SQL contains default values
    EXPECT_TRUE(create_sql.find("price REAL NOT NULL DEFAULT 0") != std::string::npos);
    EXPECT_TRUE(create_sql.find("stock INTEGER NOT NULL DEFAULT 10") != std::string::npos);
    EXPECT_TRUE(create_sql.find("active BOOLEAN NOT NULL DEFAULT true") != std::string::npos);
    EXPECT_TRUE(create_sql.find("status TEXT NOT NULL DEFAULT 'active'") != std::string::npos);
} 
