#pragma once

#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <string>
#include <optional>

// Include the literals namespace to use _fs operator
using namespace relx::literals;

/**
 * @brief Schema definitions for integration tests
 * 
 * This file contains the table definitions for the integration tests.
 * It defines the schemas for categories, products, customers, orders, and inventory.
 */
namespace schema {

// Define constant values for defaults
namespace defaults {
    static constexpr const char* pending_status = "pending";
}

/**
 * @brief Categories table schema
 */
struct Category {
    static constexpr auto table_name = "categories";
    
    relx::schema::column<Category, "id", int> id;
    relx::schema::column<Category, "name", std::string> name;
    relx::schema::column<Category, "description", std::optional<std::string>> description;
    
    // Primary key
    relx::schema::pk<&Category::id> primary;
    
    // Unique constraint
    relx::schema::unique_constraint<&Category::name> unique_name;
};

/**
 * @brief Products table schema
 */
struct Product {
    static constexpr auto table_name = "products";
    
    relx::schema::column<Product, "id", int> id;
    relx::schema::column<Product, "category_id", int> category_id;
    relx::schema::column<Product, "name", std::string> name;
    relx::schema::column<Product, "description", std::optional<std::string>> description;
    relx::schema::column<Product, "price", double> price;
    relx::schema::column<Product, "sku", std::string> sku;
    relx::schema::column<Product, "is_active", bool, relx::schema::default_value<true>> is_active;
    relx::schema::column<Product, "created_at", std::string, relx::schema::string_default<"CURRENT_TIMESTAMP">> created_at;
    
    // Primary key
    relx::schema::pk<&Product::id> primary;
    
    // Foreign key
    relx::schema::foreign_key<&Product::category_id, &Category::id> category_fk;
    
    // Unique constraint
    relx::schema::unique_constraint<&Product::sku> unique_sku;
    
    // Check constraint - using string literal directly
    relx::schema::table_check_constraint<"price > 0"> price_check;
};

/**
 * @brief Customers table schema
 */
struct Customer {
    static constexpr auto table_name = "customers";
    
    relx::schema::column<Customer, "id", int> id;
    relx::schema::column<Customer, "name", std::string> name;
    relx::schema::column<Customer, "email", std::string> email;
    relx::schema::column<Customer, "phone", std::optional<std::string>> phone;
    relx::schema::column<Customer, "is_active", bool, relx::schema::default_value<true>> is_active;
    relx::schema::column<Customer, "created_at", std::string, relx::schema::string_default<"CURRENT_TIMESTAMP">> created_at;
    
    // Primary key
    relx::schema::pk<&Customer::id> primary;
    
    // Unique constraint
    relx::schema::unique_constraint<&Customer::email> unique_email;
};

/**
 * @brief Orders table schema with composite foreign key
 */
struct Order {
    static constexpr auto table_name = "orders";
    
    relx::schema::column<Order, "id", int> id;
    relx::schema::column<Order, "customer_id", int> customer_id;
    relx::schema::column<Order, "product_id", int> product_id;
    relx::schema::column<Order, "quantity", int> quantity;
    relx::schema::column<Order, "total", double> total;
    // Status column without default value for now
    relx::schema::column<Order, "status", std::string> status;
    relx::schema::column<Order, "created_at", std::string, relx::schema::string_default<"CURRENT_TIMESTAMP">> created_at;
    
    // Primary key
    relx::schema::pk<&Order::id> primary;
    
    // Foreign keys
    relx::schema::foreign_key<&Order::customer_id, &Customer::id> customer_fk;
    relx::schema::foreign_key<&Order::product_id, &Product::id> product_fk;
    
    // Check constraint - using string literal directly
    relx::schema::table_check_constraint<"quantity > 0"> quantity_check;
    relx::schema::table_check_constraint<"status IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled')"> status_check;
};

/**
 * @brief Inventory table schema with composite primary key
 */
struct Inventory {
    static constexpr auto table_name = "inventory";
    
    relx::schema::column<Inventory, "product_id", int> product_id;
    relx::schema::column<Inventory, "warehouse_code", std::string> warehouse_code;
    relx::schema::column<Inventory, "quantity", int> quantity;
    relx::schema::column<Inventory, "last_updated", std::string, relx::schema::string_default<"CURRENT_TIMESTAMP">> last_updated;
    
    // Composite primary key
    relx::schema::pk<&Inventory::product_id, &Inventory::warehouse_code> primary;
    
    // Foreign key
    relx::schema::foreign_key<&Inventory::product_id, &Product::id> product_fk;
    
    // Check constraint - using string literal directly
    relx::schema::table_check_constraint<"quantity >= 0"> quantity_check;
};

} // namespace schema 