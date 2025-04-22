#include <gtest/gtest.h>
#include <relx/schema.hpp>

using namespace relx::schema;

// Parent tables with composite keys
struct Customer {
    static constexpr auto table_name = "customers";
    
    column<"id", int> id;
    column<"country_code", std::string> country_code;
    column<"customer_name", std::string> customer_name;
    
    // Composite primary key (id, country_code)
    pk<&Customer::id, &Customer::country_code> primary;
};

struct Product {
    static constexpr auto table_name = "products";
    
    column<"product_id", int> product_id;
    column<"sku", std::string> sku;
    column<"name", std::string> name;
    
    // Single column primary key
    pk<&Product::product_id> primary;
};

// Column-only definition for Order
struct OrderColumns {
    static constexpr auto table_name = "orders";
    
    column<"order_id", int> order_id;
    column<"customer_id", int> customer_id;
    column<"customer_country", std::string> customer_country;
    column<"product_id", int> product_id;
    column<"quantity", int> quantity;
};

TEST(CompositeForeignKeyTest, BasicCompositeForeignKey) {
    // Create constraints directly in the test
    composite_foreign_key<&OrderColumns::customer_id, &OrderColumns::customer_country, 
                        &Customer::id, &Customer::country_code> customer_fk;
    
    // Test the SQL definition of a composite foreign key
    EXPECT_EQ(customer_fk.sql_definition(), 
              "FOREIGN KEY (customer_id, customer_country) REFERENCES customers (id, country_code)");
    
    // Direct SQL testing
    std::string fk_sql = customer_fk.sql_definition();
    EXPECT_TRUE(fk_sql.find("FOREIGN KEY (customer_id, customer_country) REFERENCES customers (id, country_code)") 
                != std::string::npos);
}

TEST(CompositeForeignKeyTest, ForeignKeyWithReferenceActions) {
    // Create foreign key with custom actions using the make_fk helper
    auto fk_with_actions = make_fk<&OrderColumns::customer_id, &OrderColumns::customer_country,
                               &Customer::id, &Customer::country_code>
                               (reference_action::cascade, reference_action::set_null);
    
    // Test that actions are included in the SQL
    EXPECT_EQ(fk_with_actions.sql_definition(), 
              "FOREIGN KEY (customer_id, customer_country) REFERENCES customers (id, country_code) "
              "ON DELETE CASCADE ON UPDATE SET NULL");
}

TEST(CompositeForeignKeyTest, FkAlias) {
    // Create constraints directly in the test
    composite_foreign_key<&OrderColumns::customer_id, &OrderColumns::customer_country, 
                        &Customer::id, &Customer::country_code> customer_fk;
    foreign_key<&OrderColumns::product_id, &Product::product_id> product_fk;
    
    // Create aliased versions
    auto customer_fk_alias = make_fk<&OrderColumns::customer_id, &OrderColumns::customer_country,
                                    &Customer::id, &Customer::country_code>();
    auto product_fk_alias = make_fk<&OrderColumns::product_id, &Product::product_id>();
    
    // Test that the make_fk helper produces identical output to the explicit classes
    EXPECT_EQ(customer_fk.sql_definition(), customer_fk_alias.sql_definition());
    EXPECT_EQ(product_fk.sql_definition(), product_fk_alias.sql_definition());
    
    // Verify the make_fk helper chooses the right implementation based on parameter count
    EXPECT_EQ(customer_fk_alias.sql_definition(), 
              "FOREIGN KEY (customer_id, customer_country) REFERENCES customers (id, country_code)");
    EXPECT_EQ(product_fk_alias.sql_definition(), 
              "FOREIGN KEY (product_id) REFERENCES products (product_id)");
} 