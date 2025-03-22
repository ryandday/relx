#include <gtest/gtest.h>
#include <sqllib/schema/column.hpp>
#include <sqllib/schema/check_constraint.hpp>
#include <sqllib/schema/table.hpp>
#include <string>

using namespace sqllib::schema;

// Define constraint conditions as FixedString constants
constexpr auto valid_category_condition = FixedString("category IN ('electronics', 'books', 'clothing')");
constexpr auto books_pricing_condition = FixedString("(price < 100.0 AND category = 'books') OR category != 'books'");
constexpr auto price_quantity_condition = FixedString("price < quantity * 2.0");
constexpr auto electronics_price_condition = FixedString("(price <= 1000.0 AND category = 'electronics') OR category != 'electronics'");

// Test table with check constraints
struct Item {
    static constexpr auto name = std::string_view("items");
    
    column<"id", int> id;
    column<"item_name", std::string> item_name;
    column<"price", double> price;
    column<"quantity", int> quantity;
    column<"category", std::string> category;
    
    // Single column check constraints
    check_constraint<&Item::price, "> 0"> positive_price;
    check_constraint<&Item::quantity, ">= 0"> non_negative_quantity;
    
    // Table-level check constraint using FixedString constants
    check_constraint<nullptr, valid_category_condition> valid_category;
    table_check_constraint<books_pricing_condition> books_pricing;
};

TEST(CheckConstraintTest, SingleColumnConstraints) {
    // Test positive price constraint
    check_constraint<&Item::price, "> 0"> price_check;
    EXPECT_EQ(price_check.sql_definition(), "CHECK (price > 0)");
    
    // Test non-negative quantity constraint
    check_constraint<&Item::quantity, ">= 0"> quantity_check;
    EXPECT_EQ(quantity_check.sql_definition(), "CHECK (quantity >= 0)");
    
    // Test more complex column constraint
    check_constraint<&Item::item_name, "IS NOT NULL AND length(item_name) > 3"> name_check;
    EXPECT_EQ(name_check.sql_definition(), "CHECK (item_name IS NOT NULL AND length(item_name) > 3)");
}

TEST(CheckConstraintTest, TableLevelConstraints) {
    // Test table-level constraint using FixedString constant
    check_constraint<nullptr, valid_category_condition> category_check;
    EXPECT_EQ(category_check.sql_definition(), "CHECK (category IN ('electronics', 'books', 'clothing'))");
    
    // Test table-level check constraint alias
    table_check_constraint<price_quantity_condition> price_quantity_check;
    EXPECT_EQ(price_quantity_check.sql_definition(), "CHECK (price < quantity * 2.0)");
    
    // Test more complex multi-column constraint
    table_check_constraint<electronics_price_condition> electronics_price;
    EXPECT_EQ(electronics_price.sql_definition(), 
              "CHECK ((price <= 1000.0 AND category = 'electronics') OR category != 'electronics')");
}

TEST(CheckConstraintTest, TableWithCheckConstraints) {
    Item i;
    
    // Generate CREATE TABLE SQL with check constraints
    std::string create_sql = create_table_sql(i);
    
    // Validate SQL contains check constraints
    EXPECT_TRUE(create_sql.find("CHECK (price > 0)") != std::string::npos);
    EXPECT_TRUE(create_sql.find("CHECK (quantity >= 0)") != std::string::npos);
    EXPECT_TRUE(create_sql.find("CHECK (category IN ('electronics', 'books', 'clothing'))") != std::string::npos);
    EXPECT_TRUE(create_sql.find("CHECK ((price < 100.0 AND category = 'books') OR category != 'books')") 
                != std::string::npos);
} 