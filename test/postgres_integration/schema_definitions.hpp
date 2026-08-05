#pragma once

#include <optional>
#include <string>

#include <relx/query.hpp>
#include <relx/schema.hpp>

/**
 * @brief Schema definitions for integration tests
 *
 * This file contains the table definitions for the integration tests.
 * It defines the schemas for categories, products, customers, orders, and inventory.
 */
namespace schema {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

/**
 * @brief Categories table schema
 */
struct [[=relx::table("categories")]] Category {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::unique]] std::string name;
  std::optional<std::string> description;
};

/**
 * @brief Products table schema
 */
struct [[=relx::table("products"),
        =relx::ann::check("price > 0")]] Product {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Category::id>]] int category_id;
  std::string name;
  std::optional<std::string> description;
  double price;
  [[=relx::ann::unique]] std::string sku;
  [[=relx::default_value<true>{}]] bool is_active;
  [[=relx::default_sql<"CURRENT_TIMESTAMP">{}]] std::string created_at;
};

/**
 * @brief Customers table schema
 */
struct [[=relx::table("customers")]] Customer {
  [[=relx::ann::pk]] int id;
  std::string name;
  [[=relx::ann::unique]] std::string email;
  std::optional<std::string> phone;
  [[=relx::default_value<true>{}]] bool is_active;
  [[=relx::default_sql<"CURRENT_TIMESTAMP">{}]] std::string created_at;
};

/**
 * @brief Orders table schema, referencing both customers and products
 */
struct [[=relx::table("orders"),
        =relx::ann::check("quantity > 0"),
        =relx::ann::check(
            "status IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled')")]] Order {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Customer::id>]] int customer_id;
  [[=relx::ann::fk<^^Product::id>]] int product_id;
  int quantity;
  double total;
  // Status column without default value for now
  std::string status;
  [[=relx::default_sql<"CURRENT_TIMESTAMP">{}]] std::string created_at;
};

/**
 * @brief Inventory table schema with composite primary key
 */
struct [[=relx::table("inventory"),
        =relx::ann::composite_pk("product_id", "warehouse_code"),
        =relx::ann::check("quantity >= 0")]] Inventory {
  [[=relx::ann::fk<^^Product::id>]] int product_id;
  std::string warehouse_code;
  int quantity;
  [[=relx::default_sql<"CURRENT_TIMESTAMP">{}]] std::string last_updated;
};

// Table objects for queries and DDL
inline constexpr auto categories = relx::t<Category>;
inline constexpr auto products = relx::t<Product>;
inline constexpr auto customers = relx::t<Customer>;
inline constexpr auto orders = relx::t<Order>;
inline constexpr auto inventory = relx::t<Inventory>;

// clang-format on

}  // namespace schema
