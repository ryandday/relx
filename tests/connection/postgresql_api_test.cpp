#include <gtest/gtest.h>

#include <relx/postgresql.hpp>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>

namespace {

// Test table definition with all constraints defined in the class
struct Products {
    static constexpr auto table_name = "products";
    
    // Define columns with their types and constraints
    relx::schema::column<Products, "id", int> id;
    relx::schema::column<Products, "name", std::string> name;
    relx::schema::column<Products, "description", std::string> description;
    relx::schema::column<Products, "price", double> price;
    relx::schema::column<Products, "in_stock", bool> in_stock;
    relx::schema::column<Products, "category", std::string> category;
    
    // Define constraints
    relx::schema::table_primary_key<&Products::id> pk;
};

class PostgreSQLApiTest : public ::testing::Test {
protected:
    // Connection string for the Docker container
    std::string conn_string = "host=localhost port=5434 dbname=sqllib_test user=postgres password=postgres";
    
    void SetUp() override {
        // Clean up any existing test tables
        clean_test_table();
    }
    
    void TearDown() override {
        // Clean up after test
        clean_test_table();
    }
    
    // Helper to clean up the test table
    void clean_test_table() {
        relx::PostgreSQLConnection conn(conn_string);
        auto connect_result = conn.connect();
        if (connect_result) {
            // Use the schema's drop_table function
            Products p;
            std::string drop_sql = relx::schema::drop_table(p);
            auto drop_result = conn.execute_raw(drop_sql);
            conn.disconnect();
        }
    }
    
    // Helper to create the test table using schema
    void create_test_table(relx::Connection& conn) {
        Products p;
        // Generate CREATE TABLE SQL from the schema
        std::string create_sql = relx::schema::create_table(p);
        
        // For PostgreSQL, we need SERIAL for auto-incrementing primary keys
        // Since our schema doesn't generate this automatically, we need to modify the SQL
        create_sql = "CREATE TABLE IF NOT EXISTS " + std::string(Products::table_name) + " (\n"
            "id SERIAL PRIMARY KEY,\n"
            "name TEXT NOT NULL,\n"
            "description TEXT NOT NULL,\n"
            "price REAL NOT NULL,\n"
            "in_stock BOOLEAN NOT NULL,\n"
            "category TEXT NOT NULL\n"
            ");";
        
        // Create the table using the modified SQL
        auto create_result = conn.execute_raw(create_sql);
        
        ASSERT_TRUE(create_result) << "Failed to create table: " << create_result.error().message;
    }
};

TEST_F(PostgreSQLApiTest, TestTableCreation) {
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table using schema
    create_test_table(conn);
    
    // Verify table exists by trying to insert data
    Products p;
    auto insert_result = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Test Product", "A test product", 9.99, true, "Test")
    );
    
    ASSERT_TRUE(insert_result) << "Failed to insert test data: " << insert_result.error().message;
    
    // Clean up
    conn.disconnect();
}

TEST_F(PostgreSQLApiTest, TestInsertAndSelect) {
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    Products p;
    auto insert1 = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Laptop", "High-end laptop", 1299.99, true, "Electronics")
    );
    ASSERT_TRUE(insert1) << "Failed to insert test data 1: " << insert1.error().message;
    
    auto insert2 = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Phone", "Smartphone", 699.99, true, "Electronics")
    );
    ASSERT_TRUE(insert2) << "Failed to insert test data 2: " << insert2.error().message;
    
    auto insert3 = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Headphones", "Wireless headphones", 149.99, false, "Accessories")
    );
    ASSERT_TRUE(insert3) << "Failed to insert test data 3: " << insert3.error().message;
    
    // Test select all
    auto select_result = conn.execute(
        relx::query::select(p.id, p.name, p.price, p.category)
            .from(p)
            .order_by(p.id)
    );
    
    ASSERT_TRUE(select_result) << "Select query failed: " << select_result.error().message;
    ASSERT_EQ(3, select_result->size());
    ASSERT_EQ(4, select_result->column_count());
    
    // Check first row
    const auto& row1 = (*select_result)[0];
    auto id1 = row1.get<int>("id");
    auto name1 = row1.get<std::string>("name");
    auto price1 = row1.get<double>("price");
    
    ASSERT_TRUE(id1);
    ASSERT_TRUE(name1);
    ASSERT_TRUE(price1);
    
    EXPECT_EQ(1, *id1);
    EXPECT_EQ("Laptop", *name1);
    EXPECT_DOUBLE_EQ(1299.99, *price1);
    
    // Test select with where condition
    auto filtered_result = conn.execute(
        relx::query::select(p.id, p.name, p.price)
            .from(p)
            .where(p.category == "Electronics")
            .order_by(p.price)
    );
    
    ASSERT_TRUE(filtered_result) << "Filtered select query failed: " << filtered_result.error().message;
    ASSERT_EQ(2, filtered_result->size());
    
    // Check that we have the correct items in the filtered results
    bool found_laptop = false;
    bool found_phone = false;
    
    for (const auto& row : *filtered_result) {
        auto name = row.get<std::string>("name");
        ASSERT_TRUE(name);
        
        if (*name == "Laptop") {
            found_laptop = true;
            auto price = row.get<double>("price");
            ASSERT_TRUE(price);
            EXPECT_DOUBLE_EQ(1299.99, *price);
        } else if (*name == "Phone") {
            found_phone = true;
            auto price = row.get<double>("price");
            ASSERT_TRUE(price);
            EXPECT_DOUBLE_EQ(699.99, *price);
        }
    }
    
    EXPECT_TRUE(found_laptop);
    EXPECT_TRUE(found_phone);
    
    // Clean up
    conn.disconnect();
}

TEST_F(PostgreSQLApiTest, TestUpdate) {
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    Products p;
    auto insert_result = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Old Product", "Old description", 99.99, true, "Old Category")
    );
    ASSERT_TRUE(insert_result) << "Failed to insert test data: " << insert_result.error().message;
    
    // Update the product
    auto update_result = conn.execute(
        relx::query::update(p)
            .set(p.name, "Updated Product")
            .set(p.price, 149.99)
            .set(p.category, "New Category")
            .where(p.id == 1)
    );
    
    ASSERT_TRUE(update_result) << "Update query failed: " << update_result.error().message;
    
    // Verify the update was successful
    auto verify_result = conn.execute(
        relx::query::select(p.id, p.name, p.price, p.category)
            .from(p)
            .where(p.id == 1)
    );
    
    ASSERT_TRUE(verify_result) << "Verification query failed: " << verify_result.error().message;
    ASSERT_EQ(1, verify_result->size());
    
    const auto& row = (*verify_result)[0];
    auto name = row.get<std::string>("name");
    auto price = row.get<double>("price");
    auto category = row.get<std::string>("category");
    
    ASSERT_TRUE(name);
    ASSERT_TRUE(price);
    ASSERT_TRUE(category);
    
    EXPECT_EQ("Updated Product", *name);
    EXPECT_DOUBLE_EQ(149.99, *price);
    EXPECT_EQ("New Category", *category);
    
    // Clean up
    conn.disconnect();
}

TEST_F(PostgreSQLApiTest, TestDelete) {
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    Products p;
    auto insert1 = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Product 1", "Description 1", 10.99, true, "Category A")
    );
    ASSERT_TRUE(insert1) << "Failed to insert test data 1: " << insert1.error().message;
    
    auto insert2 = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Product 2", "Description 2", 20.99, false, "Category B")
    );
    ASSERT_TRUE(insert2) << "Failed to insert test data 2: " << insert2.error().message;
    
    auto insert3 = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Product 3", "Description 3", 30.99, true, "Category A")
    );
    ASSERT_TRUE(insert3) << "Failed to insert test data 3: " << insert3.error().message;
    
    // Verify we have 3 products
    auto count_result1 = conn.execute(
        relx::query::select(relx::query::count(p.id))
            .from(p)
    );
    ASSERT_TRUE(count_result1) << "Count query failed: " << count_result1.error().message;
    auto count1 = (*count_result1)[0].get<int>(0);
    ASSERT_TRUE(count1);
    EXPECT_EQ(3, *count1);
    
    // Delete product with id = 2
    auto delete_result = conn.execute(
        relx::query::delete_from(p)
            .where(p.id == 2)
    );
    ASSERT_TRUE(delete_result) << "Delete query failed: " << delete_result.error().message;
    
    // Verify we now have 2 products
    auto count_result2 = conn.execute(
        relx::query::select(relx::query::count(p.id))
            .from(p)
    );
    ASSERT_TRUE(count_result2) << "Count query failed: " << count_result2.error().message;
    auto count2 = (*count_result2)[0].get<int>(0);
    ASSERT_TRUE(count2);
    EXPECT_EQ(2, *count2);
    
    // Delete all products in Category A
    auto delete_result2 = conn.execute(
        relx::query::delete_from(p)
            .where(p.category == "Category A")
    );
    ASSERT_TRUE(delete_result2) << "Delete query failed: " << delete_result2.error().message;
    
    // Verify we now have 0 products
    auto count_result3 = conn.execute(
        relx::query::select(relx::query::count(p.id))
            .from(p)
    );
    ASSERT_TRUE(count_result3) << "Count query failed: " << count_result3.error().message;
    auto count3 = (*count_result3)[0].get<int>(0);
    ASSERT_TRUE(count3);
    EXPECT_EQ(0, *count3);
    
    // Clean up
    conn.disconnect();
}

TEST_F(PostgreSQLApiTest, TestTransactionsWithApi) {
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Test successful transaction
    ASSERT_TRUE(conn.begin_transaction());
    
    // Insert data in transaction
    Products p;
    auto insert1 = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Transaction Product", "Product in transaction", 55.55, true, "Transaction")
    );
    ASSERT_TRUE(insert1) << "Failed to insert in transaction: " << insert1.error().message;
    
    // Commit the transaction
    ASSERT_TRUE(conn.commit_transaction());
    
    // Verify data was committed
    auto verify_result = conn.execute(
        relx::query::select(p.id, p.name)
            .from(p)
            .where(p.category == "Transaction")
    );
    ASSERT_TRUE(verify_result) << "Verification query failed: " << verify_result.error().message;
    ASSERT_EQ(1, verify_result->size());
    
    // Test transaction rollback
    ASSERT_TRUE(conn.begin_transaction());
    
    // Insert data
    auto insert2 = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Rollback Product", "Will be rolled back", 99.99, false, "Rollback")
    );
    ASSERT_TRUE(insert2) << "Failed to insert for rollback: " << insert2.error().message;
    
    // Verify data is visible within transaction
    auto verify_in_tx = conn.execute(
        relx::query::select(relx::query::count(p.id))
            .from(p)
            .where(p.category == "Rollback")
    );
    ASSERT_TRUE(verify_in_tx) << "In-transaction verification failed: " << verify_in_tx.error().message;
    auto count_in_tx = (*verify_in_tx)[0].get<int>(0);
    ASSERT_TRUE(count_in_tx);
    EXPECT_EQ(1, *count_in_tx);
    
    // Rollback the transaction
    ASSERT_TRUE(conn.rollback_transaction());
    
    // Verify data was rolled back
    auto verify_after_rollback = conn.execute(
        relx::query::select(relx::query::count(p.id))
            .from(p)
            .where(p.category == "Rollback")
    );
    ASSERT_TRUE(verify_after_rollback) << "Post-rollback verification failed: " << verify_after_rollback.error().message;
    auto count_after_rollback = (*verify_after_rollback)[0].get<int>(0);
    ASSERT_TRUE(count_after_rollback);
    EXPECT_EQ(0, *count_after_rollback);
    
    // Clean up
    conn.disconnect();
}

TEST_F(PostgreSQLApiTest, TestPostgreSQLReturningClause) {
    relx::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Test INSERT with RETURNING clause
    Products p;
    auto insert_result = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Return Test Product", "Testing RETURNING clause", 299.99, false, "Test")
            .returning(p.id, p.name, p.price)
    );
    
    ASSERT_TRUE(insert_result) << "Insert with RETURNING failed: " << insert_result.error().message;
    ASSERT_EQ(1, insert_result->size()) << "Expected 1 row from INSERT RETURNING";
    ASSERT_EQ(3, insert_result->column_count()) << "Expected 3 columns from INSERT RETURNING";
    
    // Check returned values from INSERT
    const auto& insert_row = (*insert_result)[0];
    auto inserted_id = insert_row.get<int>("id");
    auto inserted_name = insert_row.get<std::string>("name");
    auto inserted_price = insert_row.get<double>("price");
    
    ASSERT_TRUE(inserted_id);
    ASSERT_TRUE(inserted_name);
    ASSERT_TRUE(inserted_price);
    
    EXPECT_EQ(1, *inserted_id); // First product should have ID 1
    EXPECT_EQ("Return Test Product", *inserted_name);
    EXPECT_DOUBLE_EQ(299.99, *inserted_price);
    
    // Test UPDATE with RETURNING clause
    auto update_result = conn.execute(
        relx::query::update(p)
            .set(p.name, "Updated Return Product")
            .set(p.price, 349.99)
            .set(p.category, "Updated Test")
            .where(p.id == *inserted_id)
            .returning(p.id, p.name, p.price, p.category, p.in_stock)
    );
    
    ASSERT_TRUE(update_result) << "Update with RETURNING failed: " << update_result.error().message;
    ASSERT_EQ(1, update_result->size()) << "Expected 1 row from UPDATE RETURNING";
    ASSERT_EQ(5, update_result->column_count()) << "Expected 5 columns from UPDATE RETURNING";
    
    // Check returned values from UPDATE
    const auto& update_row = (*update_result)[0];
    auto updated_id = update_row.get<int>("id");
    auto updated_name = update_row.get<std::string>("name");
    auto updated_price = update_row.get<double>("price");
    auto updated_category = update_row.get<std::string>("category");
    auto updated_in_stock = update_row.get<bool>("in_stock");
    
    ASSERT_TRUE(updated_id);
    ASSERT_TRUE(updated_name);
    ASSERT_TRUE(updated_price);
    ASSERT_TRUE(updated_category);
    ASSERT_TRUE(updated_in_stock);
    
    EXPECT_EQ(*inserted_id, *updated_id);
    EXPECT_EQ("Updated Return Product", *updated_name);
    EXPECT_DOUBLE_EQ(349.99, *updated_price);
    EXPECT_EQ("Updated Test", *updated_category);
    EXPECT_FALSE(*updated_in_stock);
    
    // Test INSERT multiple rows with RETURNING
    auto multi_insert_result = conn.execute(
        relx::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Bulk Product 1", "First bulk product", 99.99, true, "Bulk")
            .values("Bulk Product 2", "Second bulk product", 199.99, false, "Bulk")
            .returning(p.id, p.name)
    );
    
    ASSERT_TRUE(multi_insert_result) << "Multi-row insert with RETURNING failed: " << multi_insert_result.error().message;
    ASSERT_EQ(2, multi_insert_result->size()) << "Expected 2 rows from multi-row INSERT RETURNING";
    ASSERT_EQ(2, multi_insert_result->column_count()) << "Expected 2 columns from multi-row INSERT RETURNING";
    
    // Verify we received all the inserted IDs
    std::vector<int> returned_ids;
    std::vector<std::string> returned_names;
    
    for (const auto& row : *multi_insert_result) {
        auto id = row.get<int>("id");
        auto name = row.get<std::string>("name");
        ASSERT_TRUE(id);
        ASSERT_TRUE(name);
        returned_ids.push_back(*id);
        returned_names.push_back(*name);
    }
    
    EXPECT_EQ(2, returned_ids.size());
    EXPECT_EQ(2, returned_names.size());
    
    // The IDs should be 2 and 3 since we already inserted one product
    EXPECT_TRUE(std::find(returned_ids.begin(), returned_ids.end(), 2) != returned_ids.end());
    EXPECT_TRUE(std::find(returned_ids.begin(), returned_ids.end(), 3) != returned_ids.end());
    
    // The names should match what we inserted
    EXPECT_TRUE(std::find(returned_names.begin(), returned_names.end(), "Bulk Product 1") != returned_names.end());
    EXPECT_TRUE(std::find(returned_names.begin(), returned_names.end(), "Bulk Product 2") != returned_names.end());
    
    // Clean up
    conn.disconnect();
}

} // namespace 
