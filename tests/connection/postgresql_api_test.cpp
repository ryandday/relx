#include <gtest/gtest.h>

#include <sqllib/postgresql.hpp>
#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>
#include <sqllib/results.hpp>

namespace {

// Test table definition with all constraints defined in the class
struct Products {
    static constexpr auto table_name = "products";
    
    // Define columns with their types and constraints
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"description", std::string> description;
    sqllib::schema::column<"price", double> price;
    sqllib::schema::column<"in_stock", bool> in_stock;
    sqllib::schema::column<"category", std::string> category;
    
    // Define constraints
    sqllib::schema::primary_key<&Products::id> pk;
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
        sqllib::PostgreSQLConnection conn(conn_string);
        auto connect_result = conn.connect();
        if (connect_result) {
            // Use raw SQL for drop as the library doesn't have a drop_table function
            auto drop_result = conn.execute_raw("DROP TABLE IF EXISTS " + std::string(Products::table_name));
            conn.disconnect();
        }
    }
    
    // Helper to create the test table using schema
    void create_test_table(sqllib::Connection& conn) {
        Products p;
        // Generate CREATE TABLE SQL from the schema
        std::string create_sql = sqllib::schema::create_table_sql(p);
        
        // For PostgreSQL, we need SERIAL for auto-incrementing primary keys
        // Since our schema doesn't generate this automatically, we need to modify the SQL
        create_sql = "CREATE TABLE IF NOT EXISTS " + std::string(Products::table_name) + " (\n"
            "id SERIAL PRIMARY KEY,\n"
            "name TEXT NOT NULL,\n"
            "description TEXT NOT NULL,\n"
            "price REAL NOT NULL,\n"
            "in_stock INTEGER NOT NULL,\n"
            "category TEXT NOT NULL\n"
            ");";
        
        // Create the table using the modified SQL
        auto create_result = conn.execute_raw(create_sql);
        
        ASSERT_TRUE(create_result) << "Failed to create table: " << create_result.error().message;
    }
};

TEST_F(PostgreSQLApiTest, TestTableCreation) {
    sqllib::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table using schema
    create_test_table(conn);
    
    // Verify table exists by trying to insert data
    Products p;
    auto insert_result = conn.execute(
        sqllib::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Test Product", "A test product", 9.99, true, "Test")
    );
    
    ASSERT_TRUE(insert_result) << "Failed to insert test data: " << insert_result.error().message;
    
    // Clean up
    conn.disconnect();
}

TEST_F(PostgreSQLApiTest, TestInsertAndSelect) {
    sqllib::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    Products p;
    auto insert1 = conn.execute(
        sqllib::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Laptop", "High-end laptop", 1299.99, true, "Electronics")
    );
    ASSERT_TRUE(insert1) << "Failed to insert test data 1: " << insert1.error().message;
    
    auto insert2 = conn.execute(
        sqllib::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Phone", "Smartphone", 699.99, true, "Electronics")
    );
    ASSERT_TRUE(insert2) << "Failed to insert test data 2: " << insert2.error().message;
    
    auto insert3 = conn.execute(
        sqllib::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Headphones", "Wireless headphones", 149.99, false, "Accessories")
    );
    ASSERT_TRUE(insert3) << "Failed to insert test data 3: " << insert3.error().message;
    
    // Test select all
    auto select_result = conn.execute(
        sqllib::query::select(p.id, p.name, p.price, p.category)
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
        sqllib::query::select(p.id, p.name, p.price)
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
    sqllib::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    Products p;
    auto insert_result = conn.execute(
        sqllib::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Old Product", "Old description", 99.99, true, "Old Category")
    );
    ASSERT_TRUE(insert_result) << "Failed to insert test data: " << insert_result.error().message;
    
    // Update the product
    auto update_result = conn.execute(
        sqllib::query::update(p)
            .set(p.name, "Updated Product")
            .set(p.price, 149.99)
            .set(p.category, "New Category")
            .where(p.id == 1)
    );
    
    ASSERT_TRUE(update_result) << "Update query failed: " << update_result.error().message;
    
    // Verify the update was successful
    auto verify_result = conn.execute(
        sqllib::query::select(p.id, p.name, p.price, p.category)
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
    sqllib::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    Products p;
    auto insert1 = conn.execute(
        sqllib::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Product 1", "Description 1", 10.99, true, "Category A")
    );
    ASSERT_TRUE(insert1) << "Failed to insert test data 1: " << insert1.error().message;
    
    auto insert2 = conn.execute(
        sqllib::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Product 2", "Description 2", 20.99, false, "Category B")
    );
    ASSERT_TRUE(insert2) << "Failed to insert test data 2: " << insert2.error().message;
    
    auto insert3 = conn.execute(
        sqllib::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Product 3", "Description 3", 30.99, true, "Category A")
    );
    ASSERT_TRUE(insert3) << "Failed to insert test data 3: " << insert3.error().message;
    
    // Verify we have 3 products
    auto count_result1 = conn.execute(
        sqllib::query::select(sqllib::query::count(p.id))
            .from(p)
    );
    ASSERT_TRUE(count_result1) << "Count query failed: " << count_result1.error().message;
    auto count1 = (*count_result1)[0].get<int>(0);
    ASSERT_TRUE(count1);
    EXPECT_EQ(3, *count1);
    
    // Delete product with id = 2
    auto delete_result = conn.execute(
        sqllib::query::delete_from(p)
            .where(p.id == 2)
    );
    ASSERT_TRUE(delete_result) << "Delete query failed: " << delete_result.error().message;
    
    // Verify we now have 2 products
    auto count_result2 = conn.execute(
        sqllib::query::select(sqllib::query::count(p.id))
            .from(p)
    );
    ASSERT_TRUE(count_result2) << "Count query failed: " << count_result2.error().message;
    auto count2 = (*count_result2)[0].get<int>(0);
    ASSERT_TRUE(count2);
    EXPECT_EQ(2, *count2);
    
    // Delete all products in Category A
    auto delete_result2 = conn.execute(
        sqllib::query::delete_from(p)
            .where(p.category == "Category A")
    );
    ASSERT_TRUE(delete_result2) << "Delete query failed: " << delete_result2.error().message;
    
    // Verify we now have 0 products
    auto count_result3 = conn.execute(
        sqllib::query::select(sqllib::query::count(p.id))
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
    sqllib::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Test successful transaction
    ASSERT_TRUE(conn.begin_transaction());
    
    // Insert data in transaction
    Products p;
    auto insert1 = conn.execute(
        sqllib::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Transaction Product", "Product in transaction", 55.55, true, "Transaction")
    );
    ASSERT_TRUE(insert1) << "Failed to insert in transaction: " << insert1.error().message;
    
    // Commit the transaction
    ASSERT_TRUE(conn.commit_transaction());
    
    // Verify data was committed
    auto verify_result = conn.execute(
        sqllib::query::select(p.id, p.name)
            .from(p)
            .where(p.category == "Transaction")
    );
    ASSERT_TRUE(verify_result) << "Verification query failed: " << verify_result.error().message;
    ASSERT_EQ(1, verify_result->size());
    
    // Test transaction rollback
    ASSERT_TRUE(conn.begin_transaction());
    
    // Insert data
    auto insert2 = conn.execute(
        sqllib::query::insert_into(p)
            .columns(p.name, p.description, p.price, p.in_stock, p.category)
            .values("Rollback Product", "Will be rolled back", 99.99, false, "Rollback")
    );
    ASSERT_TRUE(insert2) << "Failed to insert for rollback: " << insert2.error().message;
    
    // Verify data is visible within transaction
    auto verify_in_tx = conn.execute(
        sqllib::query::select(sqllib::query::count(p.id))
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
        sqllib::query::select(sqllib::query::count(p.id))
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

} // namespace 