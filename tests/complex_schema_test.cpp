#include <gtest/gtest.h>
#include <sqllib/schema.hpp>
#include <vector>
#include <string>

using namespace sqllib::schema;

// This test demonstrates a more complex database schema with relationships
// and various constraints to test the entire system working together

// Users table
struct Users {
    static constexpr auto name = std::string_view("users");
    
    column<"id", int> id;
    column<"username", std::string> username;
    column<"email", std::string> email;
    column<"password_hash", std::string> password_hash;
    column<"created_at", std::string> created_at; // Simplified, would be timestamp in real DB
    nullable_column<"last_login", std::string> last_login;
    column<"active", bool> active;
    
    // Constraints
    primary_key<&Users::id> pk;
    sqllib::schema::index<&Users::username> username_idx{index_type::unique};
    sqllib::schema::index<&Users::email> email_idx{index_type::unique};
};

// Categories table
struct Categories {
    static constexpr auto name = std::string_view("categories");
    
    column<"id", int> id;
    column<"name", std::string> name_col;
    nullable_column<"description", std::string> description;
    nullable_column<"parent_id", int> parent_id;
    
    // Constraints
    primary_key<&Categories::id> pk;
    sqllib::schema::index<&Categories::name_col> name_idx{index_type::unique};
    foreign_key<&Categories::parent_id, &Categories::id> parent_fk{
        reference_action::set_null, reference_action::cascade
    };
};

// Products table
struct Products {
    static constexpr auto name = std::string_view("products");
    
    column<"id", int> id;
    column<"name", std::string> name_col;
    column<"sku", std::string> sku;
    column<"price", double> price;
    column<"stock", int> stock;
    nullable_column<"description", std::string> description;
    column<"category_id", int> category_id;
    column<"created_by", int> created_by;
    
    // Constraints
    primary_key<&Products::id> pk;
    sqllib::schema::index<&Products::sku> sku_idx{index_type::unique};
    sqllib::schema::index<&Products::name_col> name_idx;
    sqllib::schema::index<&Products::price> price_idx;
    foreign_key<&Products::category_id, &Categories::id> category_fk;
    foreign_key<&Products::created_by, &Users::id> user_fk;
};

// Orders table
struct Orders {
    static constexpr auto name = std::string_view("orders");
    
    column<"id", int> id;
    column<"user_id", int> user_id;
    column<"order_date", std::string> order_date; // Simplified
    column<"total", double> total;
    column<"status", std::string> status;
    
    // Constraints
    primary_key<&Orders::id> pk;
    foreign_key<&Orders::user_id, &Users::id> user_fk;
    sqllib::schema::index<&Orders::order_date> date_idx;
};

// Order_Items table - junction table for Orders and Products
struct OrderItems {
    static constexpr auto name = std::string_view("order_items");
    
    column<"order_id", int> order_id;
    column<"product_id", int> product_id;
    column<"quantity", int> quantity;
    column<"price", double> price; // Price at time of order
    
    // Constraints - composite primary key
    composite_primary_key<&OrderItems::order_id, &OrderItems::product_id> pk;
    foreign_key<&OrderItems::order_id, &Orders::id> order_fk{
        reference_action::cascade, reference_action::cascade
    };
    foreign_key<&OrderItems::product_id, &Products::id> product_fk{
        reference_action::restrict, reference_action::restrict
    };
};

// Test case for complex schema
TEST(ComplexSchemaTest, ECommerceSchema) {
    // Create instances of all tables
    Users users;
    Categories categories;
    Products products;
    Orders orders;
    OrderItems orderItems;
    
    // Generate CREATE TABLE statements for all tables
    std::string users_sql = create_table_sql(users);
    std::string categories_sql = create_table_sql(categories);
    std::string products_sql = create_table_sql(products);
    std::string orders_sql = create_table_sql(orders);
    std::string order_items_sql = create_table_sql(orderItems);
    
    // Check that all tables have the expected CREATE TABLE statements
    EXPECT_TRUE(users_sql.find("CREATE TABLE IF NOT EXISTS users") != std::string::npos);
    EXPECT_TRUE(categories_sql.find("CREATE TABLE IF NOT EXISTS categories") != std::string::npos);
    EXPECT_TRUE(products_sql.find("CREATE TABLE IF NOT EXISTS products") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("CREATE TABLE IF NOT EXISTS orders") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("CREATE TABLE IF NOT EXISTS order_items") != std::string::npos);
    
    // Check that foreign keys are properly set up
    EXPECT_TRUE(categories_sql.find("FOREIGN KEY (parent_id) REFERENCES categories (id)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("FOREIGN KEY (category_id) REFERENCES categories (id)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("FOREIGN KEY (created_by) REFERENCES users (id)") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("FOREIGN KEY (user_id) REFERENCES users (id)") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("FOREIGN KEY (order_id) REFERENCES orders (id)") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("FOREIGN KEY (product_id) REFERENCES products (id)") != std::string::npos);
    
    // Check that ON DELETE and ON UPDATE actions are present where specified
    EXPECT_TRUE(categories_sql.find("ON DELETE SET NULL ON UPDATE CASCADE") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("ON DELETE CASCADE ON UPDATE CASCADE") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("ON DELETE RESTRICT ON UPDATE RESTRICT") != std::string::npos);
    
    // Check index creation statements
    std::vector<std::string> indexes;
    indexes.push_back(users.username_idx.create_index_sql());
    indexes.push_back(users.email_idx.create_index_sql());
    indexes.push_back(categories.name_idx.create_index_sql());
    indexes.push_back(products.sku_idx.create_index_sql());
    indexes.push_back(products.name_idx.create_index_sql());
    indexes.push_back(products.price_idx.create_index_sql());
    indexes.push_back(orders.date_idx.create_index_sql());
    
    // Check that each index creation statement is properly formed
    for (const auto& idx : indexes) {
        EXPECT_TRUE(idx.find("CREATE") != std::string::npos);
        EXPECT_TRUE(idx.find("INDEX") != std::string::npos);
        EXPECT_TRUE(idx.find("ON") != std::string::npos);
    }
    
    // Check unique indexes
    EXPECT_TRUE(users.username_idx.create_index_sql().find("UNIQUE") != std::string::npos);
    EXPECT_TRUE(users.email_idx.create_index_sql().find("UNIQUE") != std::string::npos);
    EXPECT_TRUE(categories.name_idx.create_index_sql().find("UNIQUE") != std::string::npos);
    EXPECT_TRUE(products.sku_idx.create_index_sql().find("UNIQUE") != std::string::npos);
    
    // Regular indexes shouldn't have UNIQUE
    EXPECT_FALSE(products.name_idx.create_index_sql().find("UNIQUE") != std::string::npos);
    EXPECT_FALSE(products.price_idx.create_index_sql().find("UNIQUE") != std::string::npos);
    EXPECT_FALSE(orders.date_idx.create_index_sql().find("UNIQUE") != std::string::npos);
} 