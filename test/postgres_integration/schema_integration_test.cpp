#include <gtest/gtest.h>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/connection.hpp>
#include <relx/postgresql.hpp>
#include <string>
#include <memory>
#include <optional>

// Schema for our integration tests
namespace schema {

// Categories table
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

// Products table
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
    
    // Check constraint
    relx::schema::table_check_constraint<"price > 0"> price_check;
};

// Customers table
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

// Orders table with composite foreign key
struct Order {
    static constexpr auto table_name = "orders";
    
    relx::schema::column<Order, "id", int> id;
    relx::schema::column<Order, "customer_id", int> customer_id;
    relx::schema::column<Order, "product_id", int> product_id;
    relx::schema::column<Order, "quantity", int> quantity;
    relx::schema::column<Order, "total", double> total;
    relx::schema::column<Order, "status", std::string, relx::schema::string_default<"pending">> status;
    relx::schema::column<Order, "created_at", std::string, relx::schema::string_default<"CURRENT_TIMESTAMP">> created_at;
    
    // Primary key
    relx::schema::pk<&Order::id> primary;
    
    // Foreign keys
    relx::schema::foreign_key<&Order::customer_id, &Customer::id> customer_fk;
    relx::schema::foreign_key<&Order::product_id, &Product::id> product_fk;
    
    // Check constraint
    relx::schema::table_check_constraint<"quantity > 0"> quantity_check;
    relx::schema::table_check_constraint<"status IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled')"> status_check;
};

// Table with a composite primary key
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
    
    // Check constraint
    relx::schema::table_check_constraint<"quantity >= 0"> quantity_check;
};

} // namespace schema

// Test fixture for schema integration tests
class SchemaIntegrationTest : public ::testing::Test {
protected:
    using Connection = relx::connection::PostgreSQLConnection;
    
    std::unique_ptr<Connection> conn;
    
    void SetUp() override {
        // Connect to the database
        conn = std::make_unique<Connection>("host=localhost port=5434 dbname=sqllib_test user=postgres password=postgres");
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
    }
};

// Test creating tables from schema
TEST_F(SchemaIntegrationTest, CreateTables) {
    // Create instances of our table schemas
    schema::Category category;
    schema::Product product;
    schema::Customer customer;
    schema::Order order;
    schema::Inventory inventory;
    
    // Generate and execute create table statements in the correct order
    // 1. Create categories table
    std::string sql = relx::schema::create_table(category);
    auto result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create categories table: " << result.error().message;
    
    // 2. Create products table
    sql = relx::schema::create_table(product);
    result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create products table: " << result.error().message;
    
    // 3. Create customers table
    sql = relx::schema::create_table(customer);
    result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create customers table: " << result.error().message;
    
    // 4. Create orders table
    sql = relx::schema::create_table(order);
    result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create orders table: " << result.error().message;
    
    // 5. Create inventory table
    sql = relx::schema::create_table(inventory);
    result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create inventory table: " << result.error().message;
    
    // Verify tables exist by querying the database
    auto tables = conn->execute_raw("SELECT table_name FROM information_schema.tables WHERE table_schema = 'public' ORDER BY table_name");
    ASSERT_TRUE(tables) << "Failed to query tables: " << tables.error().message;
    
    auto& rows = *tables;
    for (auto& row : rows) {
        std::cerr << "row: " << row.get<std::string>(0).value_or("NULL") << std::endl;
    }
    ASSERT_EQ(5, rows.size()) << "Expected 5 tables to be created";
    
    std::vector<std::string> expected_tables = {"categories", "customers", "inventory", "orders", "products"};
    for (size_t i = 0; i < rows.size(); ++i) {
        auto table_name = rows[i].get<std::string>(0);
        ASSERT_TRUE(table_name);
        EXPECT_EQ(expected_tables[i], *table_name);
    }
}

// Test constraints are properly created
TEST_F(SchemaIntegrationTest, TableConstraints) {
    // Create instances of our table schemas
    schema::Category category;
    schema::Product product;
    schema::Customer customer;
    schema::Order order;
    schema::Inventory inventory;
    
    // Create all tables first
    std::string sql = relx::schema::create_table(category);
    auto result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create categories table: " << result.error().message;
    
    sql = relx::schema::create_table(product);
    result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create products table: " << result.error().message;
    
    sql = relx::schema::create_table(customer);
    result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create customers table: " << result.error().message;
    
    sql = relx::schema::create_table(order);
    result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create orders table: " << result.error().message;
    
    sql = relx::schema::create_table(inventory);
    result = conn->execute_raw(sql);
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
        "ORDER BY kcu.table_name, kcu.ordinal_position"
    );
    ASSERT_TRUE(result) << "Failed to query primary keys: " << result.error().message;
    // Verify primary key constraints
    auto& pk_rows = *result;
    
    // Expected primary keys (table_name, column_name)
    std::vector<std::pair<std::string, std::string>> expected_pks = {
        {"categories", "id"},
        {"customers", "id"},
        {"inventory", "product_id"},
        {"inventory", "warehouse_code"},
        {"orders", "id"},
        {"products", "id"}
    };
    
    std::cerr << "pk_rows: " << pk_rows.size() << std::endl;
    for (auto& row : pk_rows) {
        std::cerr << "row: " << row.get<std::string>(0).value_or("NULL") << std::endl;
    }
    ASSERT_EQ(expected_pks.size(), pk_rows.size()) << "Expected " << expected_pks.size() << " primary key columns";
    
    for (size_t i = 0; i < pk_rows.size(); ++i) {
        auto table_name = pk_rows[i].get<std::string>(0);
        auto column_name = pk_rows[i].get<std::string>(1);
        ASSERT_TRUE(table_name && column_name);
        EXPECT_EQ(expected_pks[i].first, *table_name);
        EXPECT_EQ(expected_pks[i].second, *column_name);
    }
    
    // Query foreign keys - using constraint_column_usage and key_column_usage to get referenced table/column
    result = conn->execute_raw(
        "SELECT kcu.table_name, kcu.column_name, ccu.table_name AS foreign_table_name, ccu.column_name AS foreign_column_name "
        "FROM information_schema.table_constraints tc "
        "JOIN information_schema.key_column_usage kcu "
        "ON tc.constraint_name = kcu.constraint_name "
        "AND tc.table_schema = kcu.table_schema "
        "JOIN information_schema.constraint_column_usage ccu "
        "ON tc.constraint_name = ccu.constraint_name "
        "AND tc.table_schema = ccu.table_schema "
        "WHERE tc.constraint_type = 'FOREIGN KEY' AND tc.table_schema = 'public' "
        "ORDER BY kcu.table_name, kcu.column_name"
    );
    ASSERT_TRUE(result) << "Failed to query foreign keys: " << result.error().message;
    
    auto& fk_rows = *result;
    
    // Expected foreign keys (table_name, column_name, foreign_table, foreign_column)
    std::vector<std::tuple<std::string, std::string, std::string, std::string>> expected_fks = {
        {"inventory", "product_id", "products", "id"},
        {"orders", "customer_id", "customers", "id"},
        {"orders", "product_id", "products", "id"},
        {"products", "category_id", "categories", "id"}
    };
    
    ASSERT_EQ(expected_fks.size(), fk_rows.size()) << "Expected " << expected_fks.size() << " foreign key relationships";
    
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
    schema::Category category;
    auto sql = relx::schema::create_table(category);
    auto result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create categories table: " << result.error().message;
    
    // Insert a category
    using namespace relx::query;
    auto insert_category = insert_into(category)
        .columns(category.id, category.name)
        .values(1, "Test Category");
    result = conn->execute_raw(insert_category.to_sql(), insert_category.bind_params());
    ASSERT_TRUE(result) << "Failed to insert category: " << result.error().message;
    
    // Create product table that references category table
    schema::Product product;
    sql = relx::schema::create_table(product);
    result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create products table: " << result.error().message;
    
    // Insert a product with minimum fields (omitting columns with defaults)
    auto insert = insert_into(product)
        .columns(product.id, product.category_id, product.name, product.price, product.sku)
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
    schema::Category category;
    auto sql = relx::schema::create_table(category);
    auto result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create categories table: " << result.error().message;
    
    // Create product table
    schema::Product product;
    sql = relx::schema::create_table(product);
    result = conn->execute_raw(sql);
    ASSERT_TRUE(result) << "Failed to create products table: " << result.error().message;
    
    // Insert a category
    using namespace relx::query;
    
    auto insert_category = insert_into(category)
        .columns(category.id, category.name)
        .values(1, "Test Category");
        
    result = conn->execute_raw(insert_category.to_sql(), insert_category.bind_params());
    ASSERT_TRUE(result) << "Failed to insert category: " << result.error().message;
    
    // Test primary key violation
    auto duplicate_pk = insert_into(category)
        .columns(category.id, category.name)
        .values(1, "Another Category");
        
    result = conn->execute_raw(duplicate_pk.to_sql(), duplicate_pk.bind_params());
    EXPECT_FALSE(result) << "Should fail due to duplicate primary key";
    
    // Test unique constraint violation
    auto duplicate_name = insert_into(category)
        .columns(category.id, category.name)
        .values(2, "Test Category");
        
    result = conn->execute_raw(duplicate_name.to_sql(), duplicate_name.bind_params());
    EXPECT_FALSE(result) << "Should fail due to duplicate name (unique constraint)";
    
    // Test foreign key constraint
    auto invalid_fk = insert_into(product)
        .columns(product.id, product.category_id, product.name, product.price, product.sku)
        .values(1, 999, "Invalid Product", 9.99, "IP001");
        
    result = conn->execute_raw(invalid_fk.to_sql(), invalid_fk.bind_params());
    EXPECT_FALSE(result) << "Should fail due to invalid foreign key";
    
    // Test check constraint
    auto invalid_price = insert_into(product)
        .columns(product.id, product.category_id, product.name, product.price, product.sku)
        .values(1, 1, "Negative Price", -1.0, "NP001");
        
    result = conn->execute_raw(invalid_price.to_sql(), invalid_price.bind_params());
    EXPECT_FALSE(result) << "Should fail due to negative price (check constraint)";
}

// Test database creation using create_table helper
TEST_F(SchemaIntegrationTest, CreateTableHelper) {
    // Create instances of our table schemas
    schema::Category category;
    schema::Product product;
    
    // Use the helper function to create a category table
    auto sql = relx::schema::create_table(category);
    auto result = conn->execute_raw(sql);
    EXPECT_TRUE(result) << "Failed to create category table with helper: " << result.error().message;
    
    // Try to create it again, which should fail
    auto sql2 = relx::schema::create_table(category, false);
    result = conn->execute_raw(sql2);
    EXPECT_FALSE(result) << "Should fail to create duplicate table";
    
    // Use if_not_exists flag to avoid errors on duplicate creation
    auto sql3 = relx::schema::create_table(category, true);
    result = conn->execute_raw(sql3);
    EXPECT_TRUE(result) << "Should succeed with if_not_exists flag: " << result.error().message;
    
    // Verify the table exists by inserting data
    using namespace relx::query;
    
    auto insert = insert_into(category)
        .columns(category.id, category.name)
        .values(1, "Test Category");
        
    result = conn->execute_raw(insert.to_sql(), insert.bind_params());
    EXPECT_TRUE(result) << "Failed to insert into category table: " << result.error().message;
    
    // Now create the product table with a dependency
    auto sql4 = relx::schema::create_table(product, false);
    result = conn->execute_raw(sql4);
    EXPECT_TRUE(result) << "Failed to create product table with helper: " << result.error().message;
    
    // Try to drop the category table (should fail due to foreign key)
    auto raw_sql = relx::schema::drop_table(category).build();
    result = conn->execute_raw(raw_sql);
    EXPECT_FALSE(result) << "Should fail to drop table with dependencies";
    
    // First drop the product table, then drop the category table
    auto raw_sql2 = relx::schema::drop_table(product).build();
    result = conn->execute_raw(raw_sql2);
    EXPECT_TRUE(result) << "Failed to drop products table: " << result.error().message;
    
    result = conn->execute_raw(raw_sql);
    EXPECT_TRUE(result) << "Failed to drop categories table: " << result.error().message;
    
    // Verify both tables are gone by checking if they exist in information_schema
    result = conn->execute_raw("SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE table_schema = 'public' AND table_name = 'categories')");
    EXPECT_TRUE(result) << "Failed to query existence of category table";
    auto category_exists = (*result)[0].get<bool>(0);
    ASSERT_TRUE(category_exists);
    EXPECT_FALSE(*category_exists) << "Category table should be dropped";
    
    result = conn->execute_raw("SELECT EXISTS (SELECT 1 FROM information_schema.tables WHERE table_schema = 'public' AND table_name = 'products')");
    EXPECT_TRUE(result) << "Failed to query existence of product table";
    auto product_exists = (*result)[0].get<bool>(0);
    ASSERT_TRUE(product_exists);
    EXPECT_FALSE(*product_exists) << "Product table should be dropped";
} 