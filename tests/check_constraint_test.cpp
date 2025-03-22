#include <gtest/gtest.h>
#include <sqllib/schema/column.hpp>
#include <sqllib/schema/check_constraint.hpp>
#include <sqllib/schema/table.hpp>
#include <string>

using namespace sqllib::schema;

// Define constraint conditions as string constants
const std::string valid_category_condition = "category IN ('electronics', 'books', 'clothing')";
const std::string books_pricing_condition = "(price < 100.0 AND category = 'books') OR category != 'books'";
const std::string price_quantity_condition = "price < quantity * 2.0";
const std::string electronics_price_condition = "(price <= 1000.0 AND category = 'electronics') OR category != 'electronics'";

// Test table with check constraints
struct Item {
    static constexpr auto table_name = "items";
    
    column<"id", int> id;
    column<"item_name", std::string> item_name;
    column<"price", double> price;
    column<"quantity", int> quantity;
    column<"category", std::string> category;
    
    // Single column check constraints - now simplified
    check_constraint positive_price{"price > 0"};
    check_constraint non_negative_quantity{"quantity >= 0"};
    
    // Table-level check constraint using string constants
    check_constraint valid_category{valid_category_condition};
    check_constraint books_pricing{books_pricing_condition};
};

// Special item struct for testing special characters in constraints
struct SpecialItem {
    static constexpr auto table_name = "special_items";
    column<"item_name", std::string> item_name;
};

// Named item struct for testing named constraints
struct NamedItem {
    static constexpr auto table_name = "named_items";
    column<"price", double> price;
    column<"quantity", int> quantity;
};

TEST(CheckConstraintTest, SingleColumnConstraints) {
    // Test positive price constraint
    check_constraint price_check{"price > 0"};
    EXPECT_EQ(price_check.sql_definition(), "CHECK (price > 0)");
    
    // Test non-negative quantity constraint
    check_constraint quantity_check{"quantity >= 0"};
    EXPECT_EQ(quantity_check.sql_definition(), "CHECK (quantity >= 0)");
    
    // Test more complex column constraint
    check_constraint name_check{"item_name IS NOT NULL AND length(item_name) > 3"};
    EXPECT_EQ(name_check.sql_definition(), "CHECK (item_name IS NOT NULL AND length(item_name) > 3)");
}

TEST(CheckConstraintTest, TableLevelConstraints) {
    // Test table-level constraint using string constant
    check_constraint category_check{valid_category_condition};
    EXPECT_EQ(category_check.sql_definition(), "CHECK (category IN ('electronics', 'books', 'clothing'))");
    
    // Test table-level check constraint 
    check_constraint price_quantity_check{price_quantity_condition};
    EXPECT_EQ(price_quantity_check.sql_definition(), "CHECK (price < quantity * 2.0)");
    
    // Test more complex multi-column constraint
    check_constraint electronics_price{electronics_price_condition};
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

// Test for handling special characters in check constraints
TEST(CheckConstraintTest, SpecialCharacters) {
    // Test with single quotes in constraint
    std::string condition_with_quotes = "item_name LIKE '%special''s item%'";
    check_constraint quotes_check{condition_with_quotes};
    EXPECT_EQ(quotes_check.sql_definition(), "CHECK (item_name LIKE '%special''s item%')");
    
    // Test with backslash and double quotes
    std::string condition_with_backslash = "item_name LIKE '%\\special\\%' OR item_name LIKE '%\"quoted\"%'";
    check_constraint backslash_check{condition_with_backslash};
    EXPECT_EQ(backslash_check.sql_definition(), "CHECK (item_name LIKE '%\\special\\%' OR item_name LIKE '%\"quoted\"%')");
    
    // Test with comparison operators and parentheses
    std::string complex_condition = "(price > 100.0 AND price <= 1000.0) OR (price = 50.0 AND category = 'sale')";
    check_constraint complex_check{complex_condition};
    EXPECT_EQ(complex_check.sql_definition(), 
              "CHECK ((price > 100.0 AND price <= 1000.0) OR (price = 50.0 AND category = 'sale'))");
    
    // Test with column constraint and special characters
    check_constraint special_name_check{"item_name LIKE '%O''Brien''s%' OR item_name LIKE '%100\\%%'"};
    EXPECT_EQ(special_name_check.sql_definition(), 
              "CHECK (item_name LIKE '%O''Brien''s%' OR item_name LIKE '%100\\%%')");
}

// Test for named check constraints
TEST(CheckConstraintTest, NamedConstraints) {
    // Test named column constraint
    check_constraint named_price_check{"price > 0", "positive_price"};
    EXPECT_EQ(named_price_check.sql_definition(), "CONSTRAINT positive_price CHECK (price > 0)");
    
    // Test named table constraint
    std::string quantity_price_condition = "quantity * price >= 1000";
    check_constraint named_table_check{quantity_price_condition, "min_order_value"};
    EXPECT_EQ(named_table_check.sql_definition(), 
              "CONSTRAINT min_order_value CHECK (quantity * price >= 1000)");
    
    // Test named table constraint with helper function
    check_constraint named_helper_check = named_check(quantity_price_condition, "min_order_value");
    EXPECT_EQ(named_helper_check.sql_definition(), 
              "CONSTRAINT min_order_value CHECK (quantity * price >= 1000)");
    
    // Test with special characters in constraint name
    check_constraint special_name_constraint{"price > 100", "premium_price_$"};
    EXPECT_EQ(special_name_constraint.sql_definition(), 
              "CONSTRAINT premium_price_$ CHECK (price > 100)");
} 