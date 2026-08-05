#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/schema.hpp>

// A realistic e-commerce schema exercising the whole annotation vocabulary at once:
// column-level pk/unique/defaults/foreign keys with actions, struct-level composite keys,
// composite uniques, table checks and indexes, across six tables that reference each other.
// Referenced tables are declared before the tables that point at them - fk<^^T::m> needs T
// to be complete.

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("users"),
        =relx::ann::check("email LIKE '%@%.%' AND length(email) > 5"),
        =relx::ann::check("status IN ('active', 'inactive', 'pending', 'suspended')"),
        =relx::ann::check("login_attempts >= 0 AND login_attempts <= 5"),
        =relx::ann::check("(active = 0 AND status = 'inactive') OR active = 1")]] Users {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::unique]] std::string username;
  [[=relx::ann::unique]] std::string email;
  std::string password_hash;
  [[=relx::default_value<false>{}]] bool email_verified;
  std::optional<std::string> profile_image;
  [[=relx::default_value<true>{}]] bool active;
  [[=relx::string_default<"active">{}]] std::string status;
  [[=relx::default_value<0>{}]] int login_attempts;
  [[=relx::string_default<"customer">{}]] std::string role;
};
inline constexpr auto users = relx::t<Users>;

// parent_id is a self-referencing FK. An annotation cannot reflect on the struct it is
// attached to, so ann::fk<^^Categories::id> is impossible here; the underlying
// references<> modifier names the target as strings and produces the same SQL.
struct [[=relx::table("categories"),
        =relx::ann::check("display_order >= 0"),
        =relx::ann::check("parent_id IS NULL OR parent_id != id")]] Categories {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::unique]] std::string name;
  std::optional<std::string> description;
  [[=relx::references<"categories", "id">{}, =relx::on_delete<"SET NULL">{},
    =relx::on_update<"CASCADE">{}]] std::optional<int> parent_id;
  [[=relx::default_value<true>{}]] bool is_active;
  [[=relx::default_value<0>{}]] int display_order;
};
inline constexpr auto categories = relx::t<Categories>;

struct [[=relx::table("products"),
        =relx::ann::composite_unique("name", "category_id"),
        =relx::ann::index_on("category_id"),
        =relx::ann::check("price >= 0 AND price <= 10000.0"),
        =relx::ann::check("stock >= 0"),
        =relx::ann::check(
            "(discount_price IS NULL) OR (discount_price < price AND discount_price >= 0)"),
        =relx::ann::check("status IN ('active', 'inactive', 'discontinued')")]] Products {
  [[=relx::ann::pk]] int id;
  std::string name;
  [[=relx::ann::unique]] std::string sku;
  [[=relx::default_value<0.0>{}]] double price;
  std::optional<double> discount_price;
  [[=relx::default_value<0>{}]] int stock;
  std::optional<std::string> description;
  [[=relx::default_value<false>{}]] bool is_featured;
  std::optional<double> weight;
  [[=relx::ann::fk<^^Categories::id>]] int category_id;
  [[=relx::ann::fk<^^Users::id>]] int created_by;
  [[=relx::string_default<"active">{}]] std::string status;
};
inline constexpr auto products = relx::t<Products>;

struct [[=relx::table("orders"),
        =relx::ann::index_on("user_id"),
        =relx::ann::check("total >= 0"),
        =relx::ann::check(
            "status IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled')"),
        =relx::ann::check(
            "(status != 'shipped' AND status != 'delivered') OR tracking_number IS NOT NULL")]]
Orders {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Users::id>]] int user_id;
  [[=relx::default_value<0.0>{}]] double total;
  [[=relx::string_default<"pending">{}]] std::string status;
  std::optional<std::string> shipping_address;
  std::optional<std::string> billing_address;
  [[=relx::string_default<"credit_card">{}]] std::string payment_method;
  [[=relx::null_default{}]] std::optional<std::string> notes;
  std::optional<std::string> tracking_number;
};
inline constexpr auto orders = relx::t<Orders>;

struct [[=relx::table("order_items"),
        =relx::ann::composite_pk("order_id", "product_id"),
        =relx::ann::check("quantity > 0"),
        =relx::ann::check("price >= 0"),
        =relx::ann::check("discount >= 0 AND discount <= price * quantity"),
        =relx::ann::check("subtotal >= 0"),
        =relx::ann::check("subtotal = (price * quantity) - discount")]] OrderItems {
  [[=relx::ann::fk<^^Orders::id>, =relx::on_delete<"CASCADE">{},
    =relx::on_update<"CASCADE">{}]] int order_id;
  [[=relx::ann::fk<^^Products::id>, =relx::on_delete<"RESTRICT">{},
    =relx::on_update<"RESTRICT">{}]] int product_id;
  [[=relx::default_value<1>{}]] int quantity;
  double price;  // price at time of order
  [[=relx::default_value<0.0>{}]] double discount;
  [[=relx::default_value<0.0>{}]] double subtotal;
  [[=relx::null_default{}]] std::optional<std::string> notes;
};
inline constexpr auto order_items = relx::t<OrderItems>;

struct [[=relx::table("customer_reviews"),
        =relx::ann::composite_unique("product_id", "user_id"),
        =relx::ann::check("rating BETWEEN 1 AND 5"),
        =relx::ann::check("helpful_votes >= 0"),
        =relx::ann::check("unhelpful_votes >= 0")]] CustomerReviews {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Products::id>]] int product_id;
  [[=relx::ann::fk<^^Users::id>]] int user_id;
  int rating;
  std::string review_text;
  [[=relx::default_value<false>{}]] bool is_verified_purchase;
  [[=relx::default_value<0>{}]] int helpful_votes;
  [[=relx::default_value<0>{}]] int unhelpful_votes;
};
inline constexpr auto reviews = relx::t<CustomerReviews>;

// Single-column pk/unique/fk render inline on the column; only constraints spanning
// columns (and the table checks) become table-level clauses after the columns.

TEST(ComplexSchemaTest, UsersDdl) {
  EXPECT_EQ(relx::create_table(users).if_not_exists().to_sql(),
            "CREATE TABLE IF NOT EXISTS users (\n"
            "id INTEGER NOT NULL PRIMARY KEY,\n"
            "username TEXT NOT NULL UNIQUE,\n"
            "email TEXT NOT NULL UNIQUE,\n"
            "password_hash TEXT NOT NULL,\n"
            "email_verified BOOLEAN NOT NULL DEFAULT false,\n"
            "profile_image TEXT,\n"
            "active BOOLEAN NOT NULL DEFAULT true,\n"
            "status TEXT NOT NULL DEFAULT 'active',\n"
            "login_attempts INTEGER NOT NULL DEFAULT 0,\n"
            "role TEXT NOT NULL DEFAULT 'customer',\n"
            "CHECK (email LIKE '%@%.%' AND length(email) > 5),\n"
            "CHECK (status IN ('active', 'inactive', 'pending', 'suspended')),\n"
            "CHECK (login_attempts >= 0 AND login_attempts <= 5),\n"
            "CHECK ((active = 0 AND status = 'inactive') OR active = 1)\n"
            ");");
}

TEST(ComplexSchemaTest, CategoriesDdlWithSelfReferencingForeignKey) {
  EXPECT_EQ(relx::create_table(categories).if_not_exists().to_sql(),
            "CREATE TABLE IF NOT EXISTS categories (\n"
            "id INTEGER NOT NULL PRIMARY KEY,\n"
            "name TEXT NOT NULL UNIQUE,\n"
            "description TEXT,\n"
            "parent_id INTEGER REFERENCES categories(id) ON DELETE SET NULL ON UPDATE CASCADE,\n"
            "is_active BOOLEAN NOT NULL DEFAULT true,\n"
            "display_order INTEGER NOT NULL DEFAULT 0,\n"
            "CHECK (display_order >= 0),\n"
            "CHECK (parent_id IS NULL OR parent_id != id)\n"
            ");");
}

TEST(ComplexSchemaTest, ProductsDdl) {
  EXPECT_EQ(
      relx::create_table(products).if_not_exists().to_sql(),
      "CREATE TABLE IF NOT EXISTS products (\n"
      "id INTEGER NOT NULL PRIMARY KEY,\n"
      "name TEXT NOT NULL,\n"
      "sku TEXT NOT NULL UNIQUE,\n"
      "price DOUBLE PRECISION NOT NULL DEFAULT 0,\n"
      "discount_price DOUBLE PRECISION,\n"
      "stock INTEGER NOT NULL DEFAULT 0,\n"
      "description TEXT,\n"
      "is_featured BOOLEAN NOT NULL DEFAULT false,\n"
      "weight DOUBLE PRECISION,\n"
      "category_id INTEGER NOT NULL REFERENCES categories(id),\n"
      "created_by INTEGER NOT NULL REFERENCES users(id),\n"
      "status TEXT NOT NULL DEFAULT 'active',\n"
      "UNIQUE (name, category_id),\n"
      "CHECK (price >= 0 AND price <= 10000.0),\n"
      "CHECK (stock >= 0),\n"
      "CHECK ((discount_price IS NULL) OR (discount_price < price AND discount_price >= 0)),\n"
      "CHECK (status IN ('active', 'inactive', 'discontinued'))\n"
      ");");
}

TEST(ComplexSchemaTest, OrdersDdl) {
  EXPECT_EQ(
      relx::create_table(orders).if_not_exists().to_sql(),
      "CREATE TABLE IF NOT EXISTS orders (\n"
      "id INTEGER NOT NULL PRIMARY KEY,\n"
      "user_id INTEGER NOT NULL REFERENCES users(id),\n"
      "total DOUBLE PRECISION NOT NULL DEFAULT 0,\n"
      "status TEXT NOT NULL DEFAULT 'pending',\n"
      "shipping_address TEXT,\n"
      "billing_address TEXT,\n"
      "payment_method TEXT NOT NULL DEFAULT 'credit_card',\n"
      "notes TEXT DEFAULT NULL,\n"
      "tracking_number TEXT,\n"
      "CHECK (total >= 0),\n"
      "CHECK (status IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled')),\n"
      "CHECK ((status != 'shipped' AND status != 'delivered') OR tracking_number IS NOT NULL)\n"
      ");");
}

TEST(ComplexSchemaTest, OrderItemsDdlWithCompositeKeyAndForeignKeyActions) {
  EXPECT_EQ(relx::create_table(order_items).if_not_exists().to_sql(),
            "CREATE TABLE IF NOT EXISTS order_items (\n"
            "order_id INTEGER NOT NULL REFERENCES orders(id) ON DELETE CASCADE ON UPDATE "
            "CASCADE,\n"
            "product_id INTEGER NOT NULL REFERENCES products(id) ON DELETE RESTRICT ON UPDATE "
            "RESTRICT,\n"
            "quantity INTEGER NOT NULL DEFAULT 1,\n"
            "price DOUBLE PRECISION NOT NULL,\n"
            "discount DOUBLE PRECISION NOT NULL DEFAULT 0,\n"
            "subtotal DOUBLE PRECISION NOT NULL DEFAULT 0,\n"
            "notes TEXT DEFAULT NULL,\n"
            "PRIMARY KEY (order_id, product_id),\n"
            "CHECK (quantity > 0),\n"
            "CHECK (price >= 0),\n"
            "CHECK (discount >= 0 AND discount <= price * quantity),\n"
            "CHECK (subtotal >= 0),\n"
            "CHECK (subtotal = (price * quantity) - discount)\n"
            ");");
}

TEST(ComplexSchemaTest, CustomerReviewsDdl) {
  EXPECT_EQ(relx::create_table(reviews).if_not_exists().to_sql(),
            "CREATE TABLE IF NOT EXISTS customer_reviews (\n"
            "id INTEGER NOT NULL PRIMARY KEY,\n"
            "product_id INTEGER NOT NULL REFERENCES products(id),\n"
            "user_id INTEGER NOT NULL REFERENCES users(id),\n"
            "rating INTEGER NOT NULL,\n"
            "review_text TEXT NOT NULL,\n"
            "is_verified_purchase BOOLEAN NOT NULL DEFAULT false,\n"
            "helpful_votes INTEGER NOT NULL DEFAULT 0,\n"
            "unhelpful_votes INTEGER NOT NULL DEFAULT 0,\n"
            "UNIQUE (product_id, user_id),\n"
            "CHECK (rating BETWEEN 1 AND 5),\n"
            "CHECK (helpful_votes >= 0),\n"
            "CHECK (unhelpful_votes >= 0)\n"
            ");");
}

// Indexes are separate statements, not part of CREATE TABLE
TEST(ComplexSchemaTest, IndexStatements) {
  constexpr auto product_indexes = relx::create_indexes_sql<Products>();
  static_assert(product_indexes.size() == 1);
  EXPECT_EQ(product_indexes[0], "CREATE INDEX products_category_id_idx ON products (category_id)");

  constexpr auto order_indexes = relx::create_indexes_sql<Orders>();
  static_assert(order_indexes.size() == 1);
  EXPECT_EQ(order_indexes[0], "CREATE INDEX orders_user_id_idx ON orders (user_id)");
}

// Tables without floating-point defaults build their whole DDL at compile time; the
// consteval and runtime builders must agree on this schema too
TEST(ComplexSchemaTest, ConstevalDdlMatchesRuntime) {
  constexpr auto users_ddl = relx::create_table_sql<Users>().if_not_exists().to_sql();
  EXPECT_EQ(users_ddl, relx::create_table(users).if_not_exists().to_sql());

  constexpr auto categories_ddl = relx::create_table_sql<Categories>().if_not_exists().to_sql();
  EXPECT_EQ(categories_ddl, relx::create_table(categories).if_not_exists().to_sql());

  constexpr auto reviews_ddl = relx::create_table_sql<CustomerReviews>().if_not_exists().to_sql();
  EXPECT_EQ(reviews_ddl, relx::create_table(reviews).if_not_exists().to_sql());
}

TEST(ComplexSchemaTest, DropTableSql) {
  EXPECT_EQ(relx::drop_table(order_items).if_exists().to_sql(),
            "DROP TABLE IF EXISTS order_items;");
}

// clang-format on

}  // namespace
