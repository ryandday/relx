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
    
    column<Users, "id", int> id;
    column<Users, "username", std::string> username;
    column<Users, "email", std::string> email;
    column<Users, "password_hash", std::string> password_hash;
    column<Users, "email_verified", bool, default_value<false>> email_verified;
    column<Users, "profile_image", std::optional<std::string>> profile_image;
    column<Users, "active", bool, default_value<true>> active;
    column<Users, "status", std::string, string_default<active_status>> status;
    column<Users, "login_attempts", int, default_value<0>> login_attempts;
    column<Users, "role", std::string, string_default<user_role>> role;
    
    // Constraints
    table_primary_key<&Users::id> pk;
    unique_constraint<&Users::username> unique_username;
    unique_constraint<&Users::email> unique_email;
    
    // Check constraints
    table_check_constraint<"email LIKE '%@%.%' AND length(email) > 5"> valid_email;
    table_check_constraint<"status IN ('active', 'inactive', 'pending', 'suspended')"> valid_status;
    table_check_constraint<"login_attempts >= 0 AND login_attempts <= 5"> valid_login;
    
    // Table-level check constraint
    table_check_constraint<"(active = 0 AND status = 'inactive') OR active = 1"> consistent_status;
};

// Categories table
struct Categories {
    static constexpr auto table_name = "categories";
    
    column<Categories, "id", int> id;
    column<Categories, "name", std::string> name_col;
    column<Categories, "description", std::optional<std::string>> description;
    column<Categories, "parent_id", std::optional<int>> parent_id;
    column<Categories, "is_active", bool, default_value<true>> is_active;
    column<Categories, "display_order", int, default_value<0>> display_order;
    
    // Constraints
    table_primary_key<&Categories::id> pk;
    unique_constraint<&Categories::name_col> unique_name;
    foreign_key<&Categories::parent_id, &Categories::id> parent_fk{
        reference_action::set_null, reference_action::cascade
    };
    
    // Check constraints
    table_check_constraint<"display_order >= 0"> valid_display_order;
    table_check_constraint<"parent_id IS NULL OR parent_id != id"> prevent_self_reference;
};

// Products table with all features
struct Products {
    static constexpr auto table_name = "products";
    
    column<Products, "id", int> id;
    column<Products, "name", std::string> name_col;
    column<Products, "sku", std::string> sku;
    column<Products, "price", double, default_value<0.0>> price;
    column<Products, "discount_price", std::optional<double>> discount_price;
    column<Products, "stock", int, default_value<0>> stock;
    column<Products, "description", std::optional<std::string>> description;
    column<Products, "is_featured", bool, default_value<false>> is_featured;
    column<Products, "weight", std::optional<double>> weight;
    column<Products, "category_id", int> category_id;
    column<Products, "created_by", int> created_by;
    column<Products, "status", std::string, string_default<active_status>> status;
    
    // Constraints
    table_primary_key<&Products::id> pk;
    unique_constraint<&Products::sku> unique_sku;
    composite_unique_constraint<&Products::name_col, &Products::category_id> unique_name_per_category;
    
    foreign_key<&Products::category_id, &Categories::id> category_fk;
    foreign_key<&Products::created_by, &Users::id> user_fk;
    
    // Check constraints
    table_check_constraint<"price >= 0 AND price <= 10000.0"> valid_price;
    table_check_constraint<"stock >= 0"> valid_stock;
    table_check_constraint<"(discount_price IS NULL) OR (discount_price < price AND discount_price >= 0)"> valid_discount;
    table_check_constraint<"status IN ('active', 'inactive', 'discontinued')"> valid_product_status;
};

// Orders table with all features
struct Orders {
    static constexpr auto table_name = "orders";
    
    column<Orders, "id", int> id;
    column<Orders, "user_id", int> user_id;
    column<Orders, "total", double, default_value<0.0>> total;
    column<Orders, "status", std::string, string_default<pending_status>> status;
    column<Orders, "shipping_address", std::optional<std::string>> shipping_address;
    column<Orders, "billing_address", std::optional<std::string>> billing_address;
    column<Orders, "payment_method", std::string, string_default<credit_card>> payment_method;
    column<Orders, "notes", std::optional<std::string>, null_default> notes;
    column<Orders, "tracking_number", std::optional<std::string>> tracking_number;
    
    // Constraints
    table_primary_key<&Orders::id> pk;
    foreign_key<&Orders::user_id, &Users::id> user_fk;
    
    // Check constraints
    table_check_constraint<"total >= 0"> valid_total;
    table_check_constraint<"status IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled')"> valid_order_status;
    table_check_constraint<"(status != 'shipped' AND status != 'delivered') OR tracking_number IS NOT NULL"> tracking_required;
};

// Order_Items table with enhanced features
struct OrderItems {
    static constexpr auto table_name = "order_items";
    
    column<OrderItems, "order_id", int> order_id;
    column<OrderItems, "product_id", int> product_id;
    column<OrderItems, "quantity", int, default_value<1>> quantity;
    column<OrderItems, "price", double> price; // Price at time of order
    column<OrderItems, "discount", double, default_value<0.0>> discount;
    column<OrderItems, "subtotal", double, default_value<0.0>> subtotal;
    column<OrderItems, "notes", std::optional<std::string>, null_default> notes;
    
    // Constraints - composite primary key
    composite_primary_key<&OrderItems::order_id, &OrderItems::product_id> pk;
    foreign_key<&OrderItems::order_id, &Orders::id> order_fk{
        reference_action::cascade, reference_action::cascade
    };
    foreign_key<&OrderItems::product_id, &Products::id> product_fk{
        reference_action::restrict, reference_action::restrict
    };
    
    // Check constraints
    table_check_constraint<"quantity > 0"> valid_quantity;
    table_check_constraint<"price >= 0"> valid_price;
    table_check_constraint<"discount >= 0 AND discount <= price * quantity"> valid_discount;
    table_check_constraint<"subtotal >= 0"> valid_subtotal;
    table_check_constraint<"subtotal = (price * quantity) - discount"> correct_subtotal;
};

// Customer Reviews table to demonstrate more features
struct CustomerReviews {
    static constexpr auto table_name = "customer_reviews";
    
    column<CustomerReviews, "id", int> id;
    column<CustomerReviews, "product_id", int> product_id;
    column<CustomerReviews, "user_id", int> user_id;
    column<CustomerReviews, "rating", int> rating;
    column<CustomerReviews, "review_text", std::string> review_text;
    column<CustomerReviews, "is_verified_purchase", bool, default_value<false>> is_verified_purchase;
    column<CustomerReviews, "helpful_votes", int, default_value<0>> helpful_votes;
    column<CustomerReviews, "unhelpful_votes", int, default_value<0>> unhelpful_votes;
    
    // Constraints
    table_primary_key<&CustomerReviews::id> pk;
    composite_unique_constraint<&CustomerReviews::product_id, &CustomerReviews::user_id> one_review_per_product;
    foreign_key<&CustomerReviews::product_id, &Products::id> product_fk;
    foreign_key<&CustomerReviews::user_id, &Users::id> user_fk;
    
    // Check constraints
    table_check_constraint<"rating BETWEEN 1 AND 5"> valid_rating;
    table_check_constraint<"helpful_votes >= 0"> valid_helpful_votes;
    table_check_constraint<"unhelpful_votes >= 0"> valid_unhelpful_votes;
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
    
    // Generate SQL for each table
    std::string users_sql = create_table(users).if_not_exists().to_sql();
    std::string categories_sql = create_table(categories).if_not_exists().to_sql();
    std::string products_sql = create_table(products).if_not_exists().to_sql();
    std::string orders_sql = create_table(orders).if_not_exists().to_sql();
    std::string order_items_sql = create_table(orderItems).if_not_exists().to_sql();
    std::string reviews_sql = create_table(reviews).if_not_exists().to_sql();
    
    // Print the SQL statements for debugging
    
    // Assertions to verify basic structure
    EXPECT_TRUE(users_sql.find("CREATE TABLE IF NOT EXISTS users") != std::string::npos);
    EXPECT_TRUE(categories_sql.find("CREATE TABLE IF NOT EXISTS categories") != std::string::npos);
    EXPECT_TRUE(products_sql.find("CREATE TABLE IF NOT EXISTS products") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("CREATE TABLE IF NOT EXISTS orders") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("CREATE TABLE IF NOT EXISTS order_items") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("CREATE TABLE IF NOT EXISTS customer_reviews") != std::string::npos);
    
    // Assertions to verify primary keys
    EXPECT_TRUE(users_sql.find("PRIMARY KEY (id)") != std::string::npos);
    EXPECT_TRUE(categories_sql.find("PRIMARY KEY (id)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("PRIMARY KEY (id)") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("PRIMARY KEY (id)") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("PRIMARY KEY (order_id, product_id)") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("PRIMARY KEY (id)") != std::string::npos);
    
    // Assertions to verify foreign keys
    EXPECT_TRUE(products_sql.find("FOREIGN KEY (category_id) REFERENCES categories(id)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("FOREIGN KEY (created_by) REFERENCES users(id)") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("FOREIGN KEY (user_id) REFERENCES users(id)") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("FOREIGN KEY (order_id) REFERENCES orders(id)") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("FOREIGN KEY (product_id) REFERENCES products(id)") != std::string::npos);
    
    // Assertions to verify unique constraints
    EXPECT_TRUE(users_sql.find("UNIQUE (username)") != std::string::npos);
    EXPECT_TRUE(users_sql.find("UNIQUE (email)") != std::string::npos);
    EXPECT_TRUE(categories_sql.find("UNIQUE (name)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("UNIQUE (sku)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("UNIQUE (name, category_id)") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("UNIQUE (product_id, user_id)") != std::string::npos);
    
    // Assertions to verify default values
    EXPECT_TRUE(users_sql.find("email_verified BOOLEAN NOT NULL DEFAULT false") != std::string::npos);
    EXPECT_TRUE(users_sql.find("active BOOLEAN NOT NULL DEFAULT true") != std::string::npos);
    EXPECT_TRUE(users_sql.find("status TEXT NOT NULL DEFAULT 'active'") != std::string::npos);
    EXPECT_TRUE(users_sql.find("login_attempts INTEGER NOT NULL DEFAULT 0") != std::string::npos);
    EXPECT_TRUE(users_sql.find("role TEXT NOT NULL DEFAULT 'customer'") != std::string::npos);
    
    EXPECT_TRUE(categories_sql.find("is_active BOOLEAN NOT NULL DEFAULT true") != std::string::npos);
    EXPECT_TRUE(categories_sql.find("display_order INTEGER NOT NULL DEFAULT 0") != std::string::npos);
    
    EXPECT_TRUE(products_sql.find("price REAL NOT NULL DEFAULT 0") != std::string::npos);
    EXPECT_FALSE(products_sql.find("discount_price") == std::string::npos);
    EXPECT_TRUE(products_sql.find("stock INTEGER NOT NULL DEFAULT 0") != std::string::npos);
    EXPECT_TRUE(products_sql.find("is_featured BOOLEAN NOT NULL DEFAULT false") != std::string::npos);
    
    EXPECT_TRUE(orders_sql.find("total REAL NOT NULL DEFAULT 0") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("status TEXT NOT NULL DEFAULT 'pending'") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("payment_method TEXT NOT NULL DEFAULT 'credit_card'") != std::string::npos);
    EXPECT_TRUE(orders_sql.find("notes TEXT DEFAULT NULL") != std::string::npos);
    
    EXPECT_TRUE(order_items_sql.find("quantity INTEGER NOT NULL DEFAULT 1") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("discount REAL NOT NULL DEFAULT 0") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("subtotal REAL NOT NULL DEFAULT 0") != std::string::npos);
    
    EXPECT_TRUE(reviews_sql.find("is_verified_purchase BOOLEAN NOT NULL DEFAULT false") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("helpful_votes INTEGER NOT NULL DEFAULT 0") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("unhelpful_votes INTEGER NOT NULL DEFAULT 0") != std::string::npos);
    
    // Assertions to verify check constraints
    EXPECT_TRUE(users_sql.find("CHECK (email LIKE '%@%.%' AND length(email) > 5)") != std::string::npos);
    EXPECT_TRUE(users_sql.find("CHECK (status IN ('active', 'inactive', 'pending', 'suspended'))") != std::string::npos);
    EXPECT_TRUE(users_sql.find("CHECK (login_attempts >= 0 AND login_attempts <= 5)") != std::string::npos);
    
    EXPECT_TRUE(categories_sql.find("CHECK (display_order >= 0)") != std::string::npos);
    EXPECT_TRUE(categories_sql.find("CHECK (parent_id IS NULL OR parent_id != id)") != std::string::npos);
    
    EXPECT_TRUE(products_sql.find("CHECK (price >= 0 AND price <= 10000.0)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("CHECK (stock >= 0)") != std::string::npos);
    EXPECT_TRUE(products_sql.find("CHECK ((discount_price IS NULL) OR (discount_price < price AND discount_price >= 0))") != std::string::npos);
    
    EXPECT_TRUE(orders_sql.find("CHECK (total >= 0)") != std::string::npos);
    
    EXPECT_TRUE(order_items_sql.find("CHECK (quantity > 0)") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("CHECK (price >= 0)") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("CHECK (discount >= 0 AND discount <= price * quantity)") != std::string::npos);
    EXPECT_TRUE(order_items_sql.find("CHECK (subtotal >= 0)") != std::string::npos);
    
    EXPECT_TRUE(reviews_sql.find("CHECK (rating BETWEEN 1 AND 5)") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("CHECK (helpful_votes >= 0)") != std::string::npos);
    EXPECT_TRUE(reviews_sql.find("CHECK (unhelpful_votes >= 0)") != std::string::npos);
} 
