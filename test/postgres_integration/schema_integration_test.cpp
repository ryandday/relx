#include "schema_definitions.hpp"

#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

// Indexes are not part of CREATE TABLE - they come from relx::create_indexes_sql<T>()
struct [[=relx::table("audit_log"),
        =relx::ann::index_on("event_type", "created_at"),
        =relx::ann::index_on("external_ref").unique()]] AuditLog {
  [[=relx::ann::pk]] int id;
  std::string event_type;
  std::string external_ref;
  std::string created_at;
};
inline constexpr auto audit_log = relx::t<AuditLog>;

// clang-format on

}  // namespace

// Test fixture for schema integration tests
class SchemaIntegrationTest : public ::testing::Test {
protected:
  using Connection = relx::connection::PostgreSQLConnection;

  std::unique_ptr<Connection> conn;

  void SetUp() override {
    // Connect to the database
    conn = std::make_unique<Connection>(
        "host=localhost port=5434 dbname=relx_test user=postgres password=postgres");
    auto result = conn->connect();
    ASSERT_TRUE(result) << "Failed to connect: " << result.error().message;

    // Clean up any existing tables
    cleanup_database();
  }

  void TearDown() override {
    if (conn && conn->is_connected()) {
      cleanup_database();
      conn->disconnect();
    }
  }

  void cleanup_database() {
    auto result = conn->execute_raw("DROP TABLE IF EXISTS orders CASCADE");
    ASSERT_TRUE(result) << "Failed to drop orders table: " << result.error().message;

    result = conn->execute_raw("DROP TABLE IF EXISTS inventory CASCADE");
    ASSERT_TRUE(result) << "Failed to drop inventory table: " << result.error().message;

    result = conn->execute_raw("DROP TABLE IF EXISTS customers CASCADE");
    ASSERT_TRUE(result) << "Failed to drop customers table: " << result.error().message;

    result = conn->execute_raw("DROP TABLE IF EXISTS products CASCADE");
    ASSERT_TRUE(result) << "Failed to drop products table: " << result.error().message;

    result = conn->execute_raw("DROP TABLE IF EXISTS categories CASCADE");
    ASSERT_TRUE(result) << "Failed to drop categories table: " << result.error().message;

    result = conn->execute_raw("DROP TABLE IF EXISTS audit_log CASCADE");
    ASSERT_TRUE(result) << "Failed to drop audit_log table: " << result.error().message;
  }
};

// Test creating tables from schema
TEST_F(SchemaIntegrationTest, CreateTables) {
  // Create instances of our table schemas
  constexpr auto category = schema::categories;
  constexpr auto product = schema::products;
  constexpr auto customer = schema::customers;
  constexpr auto order = schema::orders;
  constexpr auto inventory = schema::inventory;

  // Generate and execute create table statements in the correct order
  // 1. Create categories table
  auto create_category_sql = relx::schema::create_table(category);
  auto result = conn->execute(create_category_sql);
  ASSERT_TRUE(result) << "Failed to create categories table: " << result.error().message;

  // 2. Create products table
  auto create_product_sql = relx::schema::create_table(product);
  result = conn->execute(create_product_sql);
  ASSERT_TRUE(result) << "Failed to create products table: " << result.error().message;

  // 3. Create customers table
  auto create_customer_sql = relx::schema::create_table(customer);
  result = conn->execute(create_customer_sql);
  ASSERT_TRUE(result) << "Failed to create customers table: " << result.error().message;

  // 4. Create orders table
  auto create_order_sql = relx::schema::create_table(order);
  result = conn->execute(create_order_sql);
  ASSERT_TRUE(result) << "Failed to create orders table: " << result.error().message;

  // 5. Create inventory table
  auto create_inventory_sql = relx::schema::create_table(inventory);
  result = conn->execute(create_inventory_sql);
  ASSERT_TRUE(result) << "Failed to create inventory table: " << result.error().message;

  // Verify tables exist by querying the database
  // Scope to this test's tables: the shared relx_test database may hold others
  auto tables = conn->execute_raw(
      "SELECT table_name FROM information_schema.tables WHERE "
      "table_schema = 'public' AND table_name IN "
      "('categories', 'customers', 'inventory', 'orders', 'products') ORDER BY table_name");
  ASSERT_TRUE(tables) << "Failed to query tables: " << tables.error().message;

  auto& rows = *tables;
  ASSERT_EQ(5, rows.size()) << "Expected 5 tables to be created";

  std::vector<std::string> expected_tables = {"categories", "customers", "inventory", "orders",
                                              "products"};
  for (size_t i = 0; i < rows.size(); ++i) {
    auto table_name = rows[i].get<std::string>(0);
    ASSERT_TRUE(table_name);
    EXPECT_EQ(expected_tables[i], *table_name);
  }
}

// Test constraints are properly created
TEST_F(SchemaIntegrationTest, TableConstraints) {
  // Create instances of our table schemas
  constexpr auto category = schema::categories;
  constexpr auto product = schema::products;
  constexpr auto customer = schema::customers;
  constexpr auto order = schema::orders;
  constexpr auto inventory = schema::inventory;

  // Create all tables first
  auto create_category_sql = relx::schema::create_table(category);
  auto result = conn->execute(create_category_sql);
  ASSERT_TRUE(result) << "Failed to create categories table: " << result.error().message;

  auto create_product_sql = relx::schema::create_table(product);
  result = conn->execute(create_product_sql);
  ASSERT_TRUE(result) << "Failed to create products table: " << result.error().message;

  auto create_customer_sql = relx::schema::create_table(customer);
  result = conn->execute(create_customer_sql);
  ASSERT_TRUE(result) << "Failed to create customers table: " << result.error().message;

  auto create_order_sql = relx::schema::create_table(order);
  result = conn->execute(create_order_sql);
  ASSERT_TRUE(result) << "Failed to create orders table: " << result.error().message;

  auto create_inventory_sql = relx::schema::create_table(inventory);
  result = conn->execute(create_inventory_sql);
  ASSERT_TRUE(result) << "Failed to create inventory table: " << result.error().message;

  // Now query the constraints from information_schema
  using namespace relx::query;

  // Query primary keys
  result = conn->execute_raw(
      "SELECT kcu.table_name, kcu.column_name "
      "FROM information_schema.table_constraints tc "
      "JOIN information_schema.key_column_usage kcu "
      "ON tc.constraint_name = kcu.constraint_name "
      "AND tc.table_schema = kcu.table_schema "
      "WHERE tc.constraint_type = 'PRIMARY KEY' AND tc.table_schema = 'public' "
      "AND kcu.table_name IN ('categories', 'customers', 'inventory', 'orders', 'products') "
      "ORDER BY kcu.table_name, kcu.ordinal_position");
  ASSERT_TRUE(result) << "Failed to query primary keys: " << result.error().message;
  // Verify primary key constraints
  auto& pk_rows = *result;

  // Expected primary keys (table_name, column_name)
  std::vector<std::pair<std::string, std::string>> expected_pks = {{"categories", "id"},
                                                                   {"customers", "id"},
                                                                   {"inventory", "product_id"},
                                                                   {"inventory", "warehouse_code"},
                                                                   {"orders", "id"},
                                                                   {"products", "id"}};

  ASSERT_EQ(expected_pks.size(), pk_rows.size())
      << "Expected " << expected_pks.size() << " primary key columns";

  for (size_t i = 0; i < pk_rows.size(); ++i) {
    auto table_name = pk_rows[i].get<std::string>(0);
    auto column_name = pk_rows[i].get<std::string>(1);
    ASSERT_TRUE(table_name && column_name);
    EXPECT_EQ(expected_pks[i].first, *table_name);
    EXPECT_EQ(expected_pks[i].second, *column_name);
  }

  // Query foreign keys - using constraint_column_usage and key_column_usage to get referenced
  // table/column
  result = conn->execute_raw(
      "SELECT kcu.table_name, kcu.column_name, ccu.table_name AS foreign_table_name, "
      "ccu.column_name AS foreign_column_name "
      "FROM information_schema.table_constraints tc "
      "JOIN information_schema.key_column_usage kcu "
      "ON tc.constraint_name = kcu.constraint_name "
      "AND tc.table_schema = kcu.table_schema "
      "JOIN information_schema.constraint_column_usage ccu "
      "ON tc.constraint_name = ccu.constraint_name "
      "AND tc.table_schema = ccu.table_schema "
      "WHERE tc.constraint_type = 'FOREIGN KEY' AND tc.table_schema = 'public' "
      "AND kcu.table_name IN ('categories', 'customers', 'inventory', 'orders', 'products') "
      "ORDER BY kcu.table_name, kcu.column_name");
  ASSERT_TRUE(result) << "Failed to query foreign keys: " << result.error().message;

  auto& fk_rows = *result;

  // Expected foreign keys (table_name, column_name, foreign_table, foreign_column)
  std::vector<std::tuple<std::string, std::string, std::string, std::string>> expected_fks = {
      {"inventory", "product_id", "products", "id"},
      {"orders", "customer_id", "customers", "id"},
      {"orders", "product_id", "products", "id"},
      {"products", "category_id", "categories", "id"}};

  ASSERT_EQ(expected_fks.size(), fk_rows.size())
      << "Expected " << expected_fks.size() << " foreign key relationships";

  for (size_t i = 0; i < fk_rows.size(); ++i) {
    auto table_name = fk_rows[i].get<std::string>(0);
    auto column_name = fk_rows[i].get<std::string>(1);
    auto foreign_table = fk_rows[i].get<std::string>(2);
    auto foreign_column = fk_rows[i].get<std::string>(3);

    ASSERT_TRUE(table_name && column_name && foreign_table && foreign_column);
    EXPECT_EQ(std::get<0>(expected_fks[i]), *table_name);
    EXPECT_EQ(std::get<1>(expected_fks[i]), *column_name);
    EXPECT_EQ(std::get<2>(expected_fks[i]), *foreign_table);
    EXPECT_EQ(std::get<3>(expected_fks[i]), *foreign_column);
  }
}

// Test default values are correctly applied
TEST_F(SchemaIntegrationTest, DefaultValues) {
  // First create the categories table
  constexpr auto category = schema::categories;
  auto create_category_sql = relx::schema::create_table(category);
  auto result = conn->execute(create_category_sql);
  ASSERT_TRUE(result) << "Failed to create categories table: " << result.error().message;

  // Insert a category
  using namespace relx::query;
  auto insert_category =
      insert_into(category).columns(category.id, category.name).values(1, "Test Category");
  result = conn->execute_raw(insert_category.to_sql(), insert_category.bind_params());
  ASSERT_TRUE(result) << "Failed to insert category: " << result.error().message;

  // Create product table that references category table
  constexpr auto product = schema::products;
  auto create_product_sql = relx::schema::create_table(product);
  result = conn->execute(create_product_sql);
  ASSERT_TRUE(result) << "Failed to create products table: " << result.error().message;

  // Insert a product with minimum fields (omitting columns with defaults)
  auto insert = insert_into(product)
                    .columns(product.id, product.category_id, product.name, product.price,
                             product.sku)
                    .values(1, 1, "Test Product", 9.99, "TP001");

  result = conn->execute_raw(insert.to_sql(), insert.bind_params());
  ASSERT_TRUE(result) << "Failed to insert product: " << result.error().message;

  // Query the product to check default values
  auto select_query = select(product.id, product.name, product.is_active, product.created_at)
                          .from(product)
                          .where(product.id == 1);

  result = conn->execute_raw(select_query.to_sql(), select_query.bind_params());
  ASSERT_TRUE(result) << "Failed to select product: " << result.error().message;

  auto& rows = *result;
  ASSERT_EQ(1, rows.size()) << "Expected 1 product row";

  auto is_active = rows[0].get<bool>(2);
  auto created_at = rows[0].get<std::string>(3);

  ASSERT_TRUE(is_active);
  EXPECT_TRUE(*is_active) << "Default value for is_active should be true";

  ASSERT_TRUE(created_at);
  EXPECT_FALSE(created_at->empty()) << "Default value for created_at should not be empty";
}

// Test constraint violations are properly enforced
TEST_F(SchemaIntegrationTest, ConstraintViolation) {
  // Create categories table
  constexpr auto category = schema::categories;
  auto create_category_sql = relx::schema::create_table(category);
  auto result = conn->execute(create_category_sql);
  ASSERT_TRUE(result) << "Failed to create categories table: " << result.error().message;

  // Create product table
  constexpr auto product = schema::products;
  auto create_product_sql = relx::schema::create_table(product);
  result = conn->execute(create_product_sql);
  ASSERT_TRUE(result) << "Failed to create products table: " << result.error().message;

  // Insert a category
  using namespace relx::query;

  auto insert_category =
      insert_into(category).columns(category.id, category.name).values(1, "Test Category");

  result = conn->execute_raw(insert_category.to_sql(), insert_category.bind_params());
  ASSERT_TRUE(result) << "Failed to insert category: " << result.error().message;

  // Test primary key violation
  auto duplicate_pk =
      insert_into(category).columns(category.id, category.name).values(1, "Another Category");

  result = conn->execute_raw(duplicate_pk.to_sql(), duplicate_pk.bind_params());
  EXPECT_FALSE(result) << "Should fail due to duplicate primary key";

  // Test unique constraint violation
  auto duplicate_name =
      insert_into(category).columns(category.id, category.name).values(2, "Test Category");

  result = conn->execute_raw(duplicate_name.to_sql(), duplicate_name.bind_params());
  EXPECT_FALSE(result) << "Should fail due to duplicate name (unique constraint)";

  // Test foreign key constraint
  auto invalid_fk = insert_into(product)
                        .columns(product.id, product.category_id, product.name, product.price,
                                 product.sku)
                        .values(1, 999, "Invalid Product", 9.99, "IP001");

  result = conn->execute_raw(invalid_fk.to_sql(), invalid_fk.bind_params());
  EXPECT_FALSE(result) << "Should fail due to invalid foreign key";

  // Test check constraint
  auto invalid_price = insert_into(product)
                           .columns(product.id, product.category_id, product.name, product.price,
                                    product.sku)
                           .values(1, 1, "Negative Price", -1.0, "NP001");

  result = conn->execute_raw(invalid_price.to_sql(), invalid_price.bind_params());
  EXPECT_FALSE(result) << "Should fail due to negative price (check constraint)";
}

// Test database creation using create_table helper
TEST_F(SchemaIntegrationTest, CreateTableHelper) {
  // Create instances of our table schemas
  constexpr auto category = schema::categories;
  constexpr auto product = schema::products;

  // Use the helper function to create a category table
  auto create_category_sql = relx::schema::create_table(category);
  auto result = conn->execute(create_category_sql);
  EXPECT_TRUE(result) << "Failed to create category table with helper: " << result.error().message;

  // Try to create it again, which should fail
  auto create_category_sql2 = relx::schema::create_table(category);
  result = conn->execute(create_category_sql2);
  EXPECT_FALSE(result) << "Should fail to create duplicate table";

  // Use if_not_exists flag to avoid errors on duplicate creation
  auto create_category_sql3 = relx::schema::create_table(category).if_not_exists();
  result = conn->execute(create_category_sql3);
  EXPECT_TRUE(result) << "Should succeed with if_not_exists flag: " << result.error().message;

  // Verify the table exists by inserting data
  using namespace relx::query;

  auto insert =
      insert_into(category).columns(category.id, category.name).values(1, "Test Category");

  result = conn->execute_raw(insert.to_sql(), insert.bind_params());
  EXPECT_TRUE(result) << "Failed to insert into category table: " << result.error().message;

  // Now create the product table with a dependency
  auto create_product_sql4 = relx::schema::create_table(product).if_not_exists();
  result = conn->execute(create_product_sql4);
  EXPECT_TRUE(result) << "Failed to create product table with helper: " << result.error().message;

  // Try to drop the category table (should fail due to foreign key)
  auto drop_category_sql = relx::schema::drop_table(category);
  result = conn->execute(drop_category_sql);
  EXPECT_FALSE(result) << "Should fail to drop table with dependencies";

  // First drop the product table, then drop the category table
  auto drop_product_sql = relx::schema::drop_table(product);
  result = conn->execute(drop_product_sql);
  EXPECT_TRUE(result) << "Failed to drop products table: " << result.error().message;

  result = conn->execute(drop_category_sql);
  EXPECT_TRUE(result) << "Failed to drop categories table: " << result.error().message;

  // Verify both tables are gone by checking if they exist in information_schema
  result = conn->execute_raw("SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE "
                             "table_schema = 'public' AND table_name = 'categories')");
  EXPECT_TRUE(result) << "Failed to query existence of category table";
  auto category_exists = (*result)[0].get<bool>(0);
  ASSERT_TRUE(category_exists);
  EXPECT_FALSE(*category_exists) << "Category table should be dropped";

  result = conn->execute_raw("SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE "
                             "table_schema = 'public' AND table_name = 'products')");
  EXPECT_TRUE(result) << "Failed to query existence of product table";
  auto product_exists = (*result)[0].get<bool>(0);
  ASSERT_TRUE(product_exists);
  EXPECT_FALSE(*product_exists) << "Product table should be dropped";
}

// Indexes are not part of CREATE TABLE: each ann::index_on becomes its own CREATE INDEX
TEST_F(SchemaIntegrationTest, CreateIndexes) {
  auto create_audit_log_sql = relx::schema::create_table(audit_log);
  auto result = conn->execute(create_audit_log_sql);
  ASSERT_TRUE(result) << "Failed to create audit_log table: " << result.error().message;

  constexpr auto index_stmts = relx::create_indexes_sql<AuditLog>();
  static_assert(index_stmts.size() == 2);
  for (const auto stmt : index_stmts) {
    result = conn->execute_raw(std::string(stmt));
    ASSERT_TRUE(result) << "Failed to create index (" << stmt << "): " << result.error().message;
  }

  // Both indexes reached the catalog, named <table>_<columns>_idx
  auto indexes = conn->execute_raw(
      "SELECT indexname FROM pg_indexes WHERE schemaname = 'public' "
      "AND tablename = 'audit_log' AND indexname LIKE '%\\_idx' ORDER BY indexname");
  ASSERT_TRUE(indexes) << "Failed to query indexes: " << indexes.error().message;
  ASSERT_EQ(2, indexes->size()) << "Expected 2 indexes on audit_log";

  auto first_name = (*indexes)[0].get<std::string>(0);
  auto second_name = (*indexes)[1].get<std::string>(0);
  ASSERT_TRUE(first_name && second_name);
  EXPECT_EQ("audit_log_event_type_created_at_idx", *first_name);
  EXPECT_EQ("audit_log_external_ref_idx", *second_name);

  // The unique index is enforced by the database
  using namespace relx::query;

  auto insert_row = insert_into(audit_log)
                        .columns(audit_log.id, audit_log.event_type, audit_log.external_ref,
                                 audit_log.created_at)
                        .values(1, "created", "REF-1", "2024-01-01");
  result = conn->execute_raw(insert_row.to_sql(), insert_row.bind_params());
  ASSERT_TRUE(result) << "Failed to insert audit row: " << result.error().message;

  auto duplicate_ref = insert_into(audit_log)
                           .columns(audit_log.id, audit_log.event_type, audit_log.external_ref,
                                    audit_log.created_at)
                           .values(2, "updated", "REF-1", "2024-01-02");
  result = conn->execute_raw(duplicate_ref.to_sql(), duplicate_ref.bind_params());
  EXPECT_FALSE(result) << "Should fail due to duplicate external_ref (unique index)";
}
