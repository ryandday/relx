#include <gtest/gtest.h>
#include <relx/schema/column.hpp>
#include <relx/schema/check_constraint.hpp>
#include <relx/schema/table.hpp>
#include <string>

using namespace relx::schema;

// Test table with check constraints
struct Item {
    static constexpr auto table_name = "items";
    
    column<Item, "id", int> id;
    column<Item, "item_name", std::string> item_name;
    column<Item, "price", double> price;
    column<Item, "quantity", int> quantity;
    column<Item, "category", std::string> category;
    
    // Single column check constraints - now using compile-time strings
    table_check_constraint<"price > 0"> positive_price;
    table_check_constraint<"quantity >= 0"> non_negative_quantity;
    
    // Table-level check constraints using compile-time strings
    table_check_constraint<"category IN ('electronics', 'books', 'clothing')"> valid_category;
    table_check_constraint<"(price < 100.0 AND category = 'books') OR category != 'books'"> books_pricing;
};

// Special item struct for testing special characters in constraints
struct SpecialItem {
    static constexpr auto table_name = "special_items";
    column<SpecialItem, "item_name", std::string> item_name;
};

// Named item struct for testing named constraints
struct NamedItem {
    static constexpr auto table_name = "named_items";
    column<NamedItem, "price", double> price;
    column<NamedItem, "quantity", int> quantity;
};

TEST(CheckConstraintTest, SingleColumnConstraints) {
    // Test positive price constraint
    auto price_check = table_check<"price > 0">();
    EXPECT_EQ(price_check.sql_definition(), "CHECK (price > 0)");
    
    // Test non-negative quantity constraint
    auto quantity_check = table_check<"quantity >= 0">();
    EXPECT_EQ(quantity_check.sql_definition(), "CHECK (quantity >= 0)");
    
    // Test more complex column constraint
    auto name_check = table_check<"item_name IS NOT NULL AND length(item_name) > 3">();
    EXPECT_EQ(name_check.sql_definition(), "CHECK (item_name IS NOT NULL AND length(item_name) > 3)");
}

TEST(CheckConstraintTest, TableLevelConstraints) {
    // Test table-level constraint using compile-time string
    auto category_check = table_check<"category IN ('electronics', 'books', 'clothing')">();
    EXPECT_EQ(category_check.sql_definition(), "CHECK (category IN ('electronics', 'books', 'clothing'))");
    
    // Test table-level check constraint 
    auto price_quantity_check = table_check<"price < quantity * 2.0">();
    EXPECT_EQ(price_quantity_check.sql_definition(), "CHECK (price < quantity * 2.0)");
    
    // Test more complex multi-column constraint
    auto electronics_price = table_check<"(price <= 1000.0 AND category = 'electronics') OR category != 'electronics'">();
    EXPECT_EQ(electronics_price.sql_definition(), 
              "CHECK ((price <= 1000.0 AND category = 'electronics') OR category != 'electronics')");
}

TEST(CheckConstraintTest, TableWithCheckConstraints) {
    Item i;
    
    // Generate CREATE TABLE SQL with check constraints
    std::string create_sql = create_table(i).to_sql();
    
    // Validate SQL contains check constraints
    EXPECT_TRUE(create_sql.find("CHECK (price > 0)") != std::string::npos);
    EXPECT_TRUE(create_sql.find("CHECK (quantity >= 0)") != std::string::npos);
    EXPECT_TRUE(create_sql.find("CHECK (category IN ('electronics', 'books', 'clothing'))") != std::string::npos);
    EXPECT_TRUE(create_sql.find("CHECK ((price < 100.0 AND category = 'books') OR category != 'books')") 
                != std::string::npos);
}

// Test for handling special characters in check constraints
TEST(CheckConstraintTest, SpecialCharacters) {
    // Test with single quotes in constraint
    auto quotes_check = table_check<"item_name LIKE '%special''s item%'">();
    EXPECT_EQ(quotes_check.sql_definition(), "CHECK (item_name LIKE '%special''s item%')");
    
    // Test with backslash and double quotes
    auto backslash_check = table_check<"item_name LIKE '%\\special\\%' OR item_name LIKE '%\"quoted\"%'">();
    EXPECT_EQ(backslash_check.sql_definition(), "CHECK (item_name LIKE '%\\special\\%' OR item_name LIKE '%\"quoted\"%')");
    
    // Test with comparison operators and parentheses
    auto complex_check = table_check<"(price > 100.0 AND price <= 1000.0) OR (price = 50.0 AND category = 'sale')">();
    EXPECT_EQ(complex_check.sql_definition(), 
              "CHECK ((price > 100.0 AND price <= 1000.0) OR (price = 50.0 AND category = 'sale'))");
    
    // Test with column constraint and special characters
    auto special_name_check = table_check<"item_name LIKE '%O''Brien''s%' OR item_name LIKE '%100\\%%'">();
    EXPECT_EQ(special_name_check.sql_definition(), 
              "CHECK (item_name LIKE '%O''Brien''s%' OR item_name LIKE '%100\\%%')");
}

// Test for named check constraints
TEST(CheckConstraintTest, NamedConstraints) {
    // Test named column constraint
    auto named_price_check = named_check<"price > 0", "positive_price">();
    EXPECT_EQ(named_price_check.sql_definition(), "CONSTRAINT positive_price CHECK (price > 0)");
    
    // Test named table constraint
    auto named_table_check = named_check<"quantity * price >= 1000", "min_order_value">();
    EXPECT_EQ(named_table_check.sql_definition(), 
              "CONSTRAINT min_order_value CHECK (quantity * price >= 1000)");
    
    // Test with special characters in constraint name
    auto special_name_constraint = named_check<"price > 100", "premium_price_$">();
    EXPECT_EQ(special_name_constraint.sql_definition(), 
              "CONSTRAINT premium_price_$ CHECK (price > 100)");
} 
