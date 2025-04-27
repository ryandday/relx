#include <gtest/gtest.h>
#include <relx/schema.hpp>
#include <vector>
#include <string>
#include <optional>
#include <chrono>
#include <iostream>

using namespace relx::schema;

// This test demonstrates a more complex database schema with relationships
// and various constraints to test the entire system working together


// Define constraint conditions as string constants
const std::string valid_status_condition = "status IN ('active', 'inactive', 'suspended', 'deleted')";
const std::string valid_email_condition = "email_pattern IS NULL OR email_pattern LIKE '%@%.%'";
const std::string valid_price_condition = "price >= 0";
const std::string valid_stock_condition = "stock >= 0";
const std::string valid_order_status_condition = "status IN ('pending', 'processing', 'shipped', 'delivered', 'canceled')";
const std::string valid_quantity_condition = "quantity > 0";
const std::string order_total_condition = "total >= 0";

// Define string literals for default values
inline constexpr FixedString active_status = "active";
inline constexpr FixedString pending_status = "pending";
inline constexpr FixedString user_role = "customer";
inline constexpr FixedString credit_card = "credit_card";

// Users table with all features
struct Users {
    static constexpr auto table_name = "users";
    
    column<"id", int> id;
    column<"username", std::string> username;
    column<"email", std::string> email;
    column<"password_hash", std::string> password_hash;
    column<"email_verified", bool, DefaultValue<false>> email_verified;
    column<"profile_image", std::optional<std::string>> profile_image;
    column<"active", bool, DefaultValue<true>> active;
    column<"status", std::string, DefaultValue<active_status>> status;
    column<"login_attempts", int, DefaultValue<0>> login_attempts;
    column<"role", std::string, DefaultValue<user_role>> role;
    
    // Constraints
    table_primary_key<&Users::id> pk;
    unique_constraint<&Users::username> unique_username;
    unique_constraint<&Users::email> unique_email;
    
    // Check constraints
    check_constraint<"email LIKE '%@%.%' AND length(email) > 5"> valid_email;
    check_constraint<"status IN ('active', 'inactive', 'pending', 'suspended')"> valid_status;
    check_constraint<"login_attempts >= 0 AND login_attempts <= 5"> valid_login;
    
    // Table-level check constraint
    check_constraint<"(active = 0 AND status = 'inactive') OR active = 1"> consistent_status;
};

// Categories table
struct Categories {
    static constexpr auto table_name = "categories";
    
    column<"id", int> id;
    column<"name", std::string> name_col;
    column<"description", std::optional<std::string>> description;
    column<"parent_id", std::optional<int>> parent_id;
    column<"is_active", bool, DefaultValue<true>> is_active;
    column<"display_order", int, DefaultValue<0>> display_order;
    
    // Constraints
    table_primary_key<&Categories::id> pk;
    unique_constraint<&Categories::name_col> unique_name;
    foreign_key<&Categories::parent_id, &Categories::id> parent_fk{
        reference_action::set_null, reference_action::cascade
    };
    
    // Check constraints
    check_constraint<"display_order >= 0"> valid_display_order;
    check_constraint<"parent_id IS NULL OR parent_id != id"> prevent_self_reference;
};

// Products table with all features
struct Products {
    static constexpr auto table_name = "products";
    
    column<"id", int> id;
    column<"name", std::string> name_col;
    column<"sku", std::string> sku;
    column<"price", double, DefaultValue<0.0>> price;
    column<"discount_price", std::optional<double>> discount_price;
    column<"stock", int, DefaultValue<0>> stock;
    column<"description", std::optional<std::string>> description;
    column<"is_featured", bool, DefaultValue<false>> is_featured;
    column<"weight", std::optional<double>> weight;
    column<"category_id", int> category_id;
    column<"created_by", int> created_by;
    column<"status", std::string, DefaultValue<active_status>> status;
    
    // Constraints
    table_primary_key<&Products::id> pk;
    unique_constraint<&Products::sku> unique_sku;
    composite_unique_constraint<&Products::name_col, &Products::category_id> unique_name_per_category;
    
    foreign_key<&Products::category_id, &Categories::id> category_fk;
    foreign_key<&Products::created_by, &Users::id> user_fk;
    
    // Check constraints
    check_constraint<"price >= 0 AND price <= 10000.0"> valid_price;
    check_constraint<"stock >= 0"> valid_stock;
    check_constraint<"(discount_price IS NULL) OR (discount_price < price AND discount_price >= 0)"> valid_discount;
    check_constraint<"status IN ('active', 'inactive', 'discontinued')"> valid_product_status;
};

// Orders table with all features
struct Orders {
    static constexpr auto table_name = "orders";
    
    column<"id", int> id;
    column<"user_id", int> user_id;
    column<"total", double, DefaultValue<0.0>> total;
    column<"status", std::string, DefaultValue<pending_status>> status;
    column<"shipping_address", std::optional<std::string>> shipping_address;
    column<"billing_address", std::optional<std::string>> billing_address;
    column<"payment_method", std::string, DefaultValue<credit_card>> payment_method;
    column<"notes", std::optional<std::string>, null_default> notes;
    column<"tracking_number", std::optional<std::string>> tracking_number;
    
    // Constraints
    table_primary_key<&Orders::id> pk;
    foreign_key<&Orders::user_id, &Users::id> user_fk;
    
    // Check constraints
    check_constraint<"total >= 0"> valid_total;
    check_constraint<"status IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled')"> valid_order_status;
    check_constraint<"(status != 'shipped' AND status != 'delivered') OR tracking_number IS NOT NULL"> tracking_required;
};

// Order_Items table with enhanced features
struct OrderItems {
    static constexpr auto table_name = "order_items";
    
    column<"order_id", int> order_id;
    column<"product_id", int> product_id;
    column<"quantity", int, DefaultValue<1>> quantity;
    column<"price", double> price; // Price at time of order
    column<"discount", double, DefaultValue<0.0>> discount;
    column<"subtotal", double, DefaultValue<0.0>> subtotal;
    column<"notes", std::optional<std::string>, null_default> notes;
    
    // Constraints - composite primary key
    composite_primary_key<&OrderItems::order_id, &OrderItems::product_id> pk;
    foreign_key<&OrderItems::order_id, &Orders::id> order_fk{
        reference_action::cascade, reference_action::cascade
    };
    foreign_key<&OrderItems::product_id, &Products::id> product_fk{
        reference_action::restrict, reference_action::restrict
    };
    
    // Check constraints
    check_constraint<"quantity > 0"> valid_quantity;
    check_constraint<"price >= 0"> valid_price;
    check_constraint<"discount >= 0 AND discount <= price * quantity"> valid_discount;
    check_constraint<"subtotal >= 0"> valid_subtotal;
    check_constraint<"subtotal = (price * quantity) - discount"> correct_subtotal;
};

// Customer Reviews table to demonstrate more features
struct CustomerReviews {
    static constexpr auto table_name = "customer_reviews";
    
    column<"id", int> id;
    column<"product_id", int> product_id;
    column<"user_id", int> user_id;
    column<"rating", int> rating;
    column<"review_text", std::string> review_text;
    column<"is_verified_purchase", bool, DefaultValue<false>> is_verified_purchase;
    column<"helpful_votes", int, DefaultValue<0>> helpful_votes;
    column<"unhelpful_votes", int, DefaultValue<0>> unhelpful_votes;
    
    // Constraints
    table_primary_key<&CustomerReviews::id> pk;
    composite_unique_constraint<&CustomerReviews::product_id, &CustomerReviews::user_id> one_review_per_product;
    foreign_key<&CustomerReviews::product_id, &Products::id> product_fk;
    foreign_key<&CustomerReviews::user_id, &Users::id> user_fk;
    
    // Check constraints
    check_constraint<"rating BETWEEN 1 AND 5"> valid_rating;
    check_constraint<"helpful_votes >= 0"> valid_helpful_votes;
    check_constraint<"unhelpful_votes >= 0"> valid_unhelpful_votes;
};

// Test case for complex schema
TEST(ComplexSchemaTest, EnhancedECommerceSchema) {
    // Create instances of all tables
    Users users;
    Categories categories;
    Products products;
    Orders orders;
    OrderItems orderItems;
    CustomerReviews reviews;
    
    // Debug check constraint
    std::cout << "Debug check_constraint: " << users.valid_email.sql_definition() << std::endl;
    std::cout << "Check pattern: " << valid_email_condition << std::endl;
    
    // Generate CREATE TABLE statements for all tables
    std::string users_sql = create_table(users);
    std::string categories_sql = create_table(categories);
    std::string products_sql = create_table(products);
    std::string orders_sql = create_table(orders);
    std::string order_items_sql = create_table(orderItems);
    std::string reviews_sql = create_table(reviews);
    
    // Print the actual SQL for debugging
    std::cout << "USERS SQL:\n" << users_sql << std::endl;
    
    // Check table creation statements
    EXPECT_TRUE(users_sql.find("CREATE TABLE IF NOT EXISTS users") != std::string::npos);
    EXPECT_TRUE(categories_sql.find("CREATE TABLE IF NOT EXISTS categories") != std::string::npos);
    EXPECT_TRUE(products_sql.find("CREATE TABLE IF NOT EXISTS products") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("CREATE TABLE IF NOT EXISTS orders") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("CREATE TABLE IF NOT EXISTS order_items") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("CREATE TABLE IF NOT EXISTS customer_reviews") != std::string::npos);
    
    // 1. Test default values in CREATE TABLE statements
    EXPECT_TRUE(users_sql.find("login_attempts INTEGER NOT NULL DEFAULT 0") != std::string::npos);
    EXPECT_TRUE(users_sql.find("active INTEGER NOT NULL DEFAULT 1") != std::string::npos);
    EXPECT_TRUE(users_sql.find("status TEXT NOT NULL DEFAULT 'active'") != std::string::npos);
    EXPECT_TRUE(products_sql.find("price REAL NOT NULL DEFAULT 0.000000") != std::string::npos);
    EXPECT_TRUE(products_sql.find("is_featured INTEGER NOT NULL DEFAULT 0") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("total REAL NOT NULL DEFAULT 0.000000") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("status TEXT NOT NULL DEFAULT 'pending'") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("quantity INTEGER NOT NULL DEFAULT 1") != std::string::npos);
    
    // 2. Test NULL defaults
    EXPECT_TRUE(orders_sql.find("notes TEXT DEFAULT NULL") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("notes TEXT DEFAULT NULL") != std::string::npos);
    
    // 3. Test CHECK constraints - flexible assertion that accepts the actual SQL format
    // We're now testing for the actual format the code generates rather than rigid expectations
    
    // Check for email validation constraint - update to match the actual constraint
    EXPECT_TRUE(users_sql.find("CHECK (email LIKE '%@%.%' AND length(email) > 5)") != std::string::npos);
    
    // Check for status constraint - update to match the actual constraint
    EXPECT_TRUE(users_sql.find("CHECK (status IN ('active', 'inactive', 'pending', 'suspended'))") != std::string::npos);
    
    // Check for login_attempts constraint - accept the actual output
    EXPECT_TRUE(users_sql.find("CHECK (login_attempts >= 0 AND login_attempts <= 5)") != std::string::npos);
    
    // 4. Test UNIQUE constraints
    EXPECT_TRUE(users_sql.find("UNIQUE (username)") != std::string::npos);
    EXPECT_TRUE(users_sql.find("UNIQUE (email)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("UNIQUE (sku)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("UNIQUE (name, category_id)") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("UNIQUE (product_id, user_id)") != std::string::npos);
    
    // 5. Test foreign keys
    EXPECT_TRUE(categories_sql.find("FOREIGN KEY (parent_id) REFERENCES categories (id)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("FOREIGN KEY (category_id) REFERENCES categories (id)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("FOREIGN KEY (created_by) REFERENCES users (id)") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("FOREIGN KEY (user_id) REFERENCES users (id)") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("FOREIGN KEY (order_id) REFERENCES orders (id)") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("FOREIGN KEY (product_id) REFERENCES products (id)") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("FOREIGN KEY (product_id) REFERENCES products (id)") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("FOREIGN KEY (user_id) REFERENCES users (id)") != std::string::npos);
    
    // 6. Test ON DELETE and ON UPDATE actions
    EXPECT_TRUE(categories_sql.find("ON DELETE SET NULL ON UPDATE CASCADE") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("ON DELETE CASCADE ON UPDATE CASCADE") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("ON DELETE RESTRICT ON UPDATE RESTRICT") != std::string::npos);
    
    // Create some additional test cases to fully exercise the schema system
    // Test accessing default values programmatically
    auto price_default = products.price.get_default_value();
    ASSERT_TRUE(price_default.has_value());
    EXPECT_DOUBLE_EQ(*price_default, 0.0);
    
    auto is_featured_default = products.is_featured.get_default_value();
    ASSERT_TRUE(is_featured_default.has_value());
    EXPECT_FALSE(*is_featured_default);
    
    auto login_attempts_default = users.login_attempts.get_default_value();
    ASSERT_TRUE(login_attempts_default.has_value());
    EXPECT_EQ(*login_attempts_default, 0);
    
    // Test nullable columns with and without defaults
    EXPECT_TRUE(products.discount_price.nullable);
    EXPECT_FALSE(products.discount_price.has_default);
    
    // Verify that NOT NULL is not added for nullable columns
    EXPECT_FALSE(products_sql.find("discount_price REAL NOT NULL") != std::string::npos);
    EXPECT_TRUE(products_sql.find("discount_price REAL") != std::string::npos);
} 
