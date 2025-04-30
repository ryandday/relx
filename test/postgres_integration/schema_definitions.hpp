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
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"name", std::string> name;
    relx::schema::column<"description", std::optional<std::string>> description;
    
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
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"category_id", int> category_id;
    relx::schema::column<"name", std::string> name;
    relx::schema::column<"description", std::optional<std::string>> description;
    relx::schema::column<"price", double> price;
    relx::schema::column<"sku", std::string> sku;
    relx::schema::column<"is_active", bool, relx::schema::DefaultValue<true>> is_active;
    relx::schema::column<"created_at", std::string, relx::schema::DefaultValue<relx::schema::current_timestamp>> created_at;
    
    // Primary key
    relx::schema::pk<&Product::id> primary;
    
    // Foreign key
    relx::schema::foreign_key<&Product::category_id, &Category::id> category_fk;
    
    // Unique constraint
    relx::schema::unique_constraint<&Product::sku> unique_sku;
    
    // Check constraint - using string literal directly
    relx::schema::check_constraint<"price > 0"> price_check;
};

/**
 * @brief Customers table schema
 */
struct Customer {
    static constexpr auto table_name = "customers";
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"name", std::string> name;
    relx::schema::column<"email", std::string> email;
    relx::schema::column<"phone", std::optional<std::string>> phone;
    relx::schema::column<"is_active", bool, relx::schema::DefaultValue<true>> is_active;
    relx::schema::column<"created_at", std::string, relx::schema::DefaultValue<relx::schema::current_timestamp>> created_at;
    
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
    
    relx::schema::column<"id", int> id;
    relx::schema::column<"customer_id", int> customer_id;
    relx::schema::column<"product_id", int> product_id;
    relx::schema::column<"quantity", int> quantity;
    relx::schema::column<"total", double> total;
    // Status column without default value for now
    relx::schema::column<"status", std::string> status;
    relx::schema::column<"created_at", std::string, relx::schema::DefaultValue<relx::schema::current_timestamp>> created_at;
    
    // Primary key
    relx::schema::pk<&Order::id> primary;
    
    // Foreign keys
    relx::schema::foreign_key<&Order::customer_id, &Customer::id> customer_fk;
    relx::schema::foreign_key<&Order::product_id, &Product::id> product_fk;
    
    // Check constraint - using string literal directly
    relx::schema::check_constraint<"quantity > 0"> quantity_check;
    relx::schema::table_check_constraint<"status IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled')"> status_check;
};

/**
 * @brief Inventory table schema with composite primary key
 */
struct Inventory {
    static constexpr auto table_name = "inventory";
    
    relx::schema::column<"product_id", int> product_id;
    relx::schema::column<"warehouse_code", std::string> warehouse_code;
    relx::schema::column<"quantity", int> quantity;
    relx::schema::column<"last_updated", std::string, relx::schema::DefaultValue<relx::schema::current_timestamp>> last_updated;
    
    // Composite primary key
    relx::schema::pk<&Inventory::product_id, &Inventory::warehouse_code> primary;
    
    // Foreign key
    relx::schema::foreign_key<&Inventory::product_id, &Product::id> product_fk;
    
    // Check constraint - using string literal directly
    relx::schema::check_constraint<"quantity >= 0"> quantity_check;
};

} // namespace schema 