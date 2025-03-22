#include <gtest/gtest.h>
#include <sqllib/schema.hpp>

using namespace sqllib::schema;

// Test table column definitions
struct ProductColumns {
    static constexpr auto table_name = "products";
    
    column<"id", int> id;
    column<"name", std::string> name;
    column<"price", double> price;
    column<"stock", int> stock;
};

TEST(ConstraintAliasTest, PrimaryKeyAlias) {
    // Create the primary key directly in the test
    pk<&ProductColumns::id> id_pk;
    
    // Test the primary key alias produces valid SQL
    EXPECT_EQ(id_pk.sql_definition(), "PRIMARY KEY (id)");
    
    // Create both types to compare
    pk<&ProductColumns::id> pk_alias;
    primary_key<&ProductColumns::id> pk_direct;
    
    // Verify they produce the same output
    EXPECT_EQ(pk_alias.sql_definition(), pk_direct.sql_definition());
    
    // Test with a composite key
    pk<&ProductColumns::id, &ProductColumns::name> composite_pk;
    composite_primary_key<&ProductColumns::id, &ProductColumns::name> composite_pk_direct;
    
    // Verify the composite versions match
    EXPECT_EQ(composite_pk.sql_definition(), composite_pk_direct.sql_definition());
    EXPECT_EQ(composite_pk.sql_definition(), "PRIMARY KEY (id, name)");
}

TEST(ConstraintAliasTest, ColumnCheckConstraint) {
    // Create the constraints directly in the test
    column_check_constraint<&ProductColumns::price, "price > 0"> price_positive;
    column_check_constraint<&ProductColumns::stock, "stock >= 0", "positive_stock"> stock_positive;
    
    // Test column-bound check constraints work correctly
    EXPECT_EQ(price_positive.sql_definition(), "CHECK (price > 0)");
    EXPECT_EQ(stock_positive.sql_definition(), "CONSTRAINT positive_stock CHECK (stock >= 0)");
    
    // Test that column name is available
    EXPECT_EQ(price_positive.column_name(), "price");
    EXPECT_EQ(stock_positive.column_name(), "stock");
    
    // Test helper functions
    auto check1 = column_check<&ProductColumns::price, "price > 10">();
    EXPECT_EQ(check1.sql_definition(), "CHECK (price > 10)");
    
    auto check2 = named_column_check<&ProductColumns::stock, "stock < 100", "stock_limit">();
    EXPECT_EQ(check2.sql_definition(), "CONSTRAINT stock_limit CHECK (stock < 100)");
}

// Test a table with a composite primary key using the pk alias
struct CompositeKeyColumns {
    static constexpr auto table_name = "composite_products";
    
    column<"category", std::string> category;
    column<"product_code", std::string> product_code;
    column<"name", std::string> name;
    column<"price", double> price;
};

TEST(ConstraintAliasTest, CompositePrimaryKeyAlias) {
    // Create the composite primary key directly in the test
    pk<&CompositeKeyColumns::category, &CompositeKeyColumns::product_code> product_pk;
    
    // Test the composite primary key alias produces valid SQL
    EXPECT_EQ(product_pk.sql_definition(), "PRIMARY KEY (category, product_code)");
    
    // Direct constraint testing
    std::string pk_sql = product_pk.sql_definition();
    EXPECT_TRUE(pk_sql.find("PRIMARY KEY (category, product_code)") != std::string::npos);
} 