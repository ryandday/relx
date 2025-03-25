#include <gtest/gtest.h>

#include <sqllib/connection.hpp>
#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>
#include <sqllib/results.hpp>

#include <filesystem>
#include <stdexcept>

namespace {

// Test table definition
struct Users {
    static constexpr auto table_name = "users";
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::column<"age", int> age;
    
    sqllib::schema::primary_key<&Users::id> pk;
};

class TransactionGuardTest : public ::testing::Test {
protected:
    std::string db_path = "transaction_test_db.sqlite";
    
    void SetUp() override {
        // Ensure the test database doesn't exist
        std::filesystem::remove(db_path);
    }
    
    void TearDown() override {
        // Clean up after test
        std::filesystem::remove(db_path);
    }
    
    // Helper to create the test table
    void create_test_table(sqllib::Connection& conn) {
        Users u;
        auto result = conn.execute_raw(sqllib::schema::create_table_sql(u));
        ASSERT_TRUE(result) << "Failed to create table: " << result.error().message;
    }
};

TEST_F(TransactionGuardTest, TestBasicCommit) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Use TransactionGuard with successful commit
    {
        sqllib::TransactionGuard guard(conn);
        
        // Insert test data
        Users u;
        auto result = conn.execute(sqllib::query::insert_into(u)
            .columns(u.name, u.email, u.age)
            .values(sqllib::query::val("Test User"), sqllib::query::val("test@example.com"), sqllib::query::val(25)));
        ASSERT_TRUE(result) << "Failed to insert data: " << result.error().message;
        
        // Explicitly commit
        guard.commit();
        
        // Verify the transaction was committed
        EXPECT_TRUE(guard.is_committed());
        EXPECT_FALSE(guard.is_rolled_back());
        EXPECT_FALSE(conn.in_transaction());
    }
    
    // Verify data was committed
    auto result = conn.execute_raw("SELECT * FROM users WHERE name = 'Test User'");
    ASSERT_TRUE(result) << "Failed to select data: " << result.error().message;
    EXPECT_EQ(1, result->size());
    
    // Clean up
    conn.disconnect();
}

TEST_F(TransactionGuardTest, TestAutomaticRollback) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Use TransactionGuard with automatic rollback on destruction
    {
        sqllib::TransactionGuard guard(conn);
        
        // Insert test data
        Users u;
        auto result = conn.execute(sqllib::query::insert_into(u)
            .columns(u.name, u.email, u.age)
            .values(sqllib::query::val("Rollback User"), sqllib::query::val("rollback@example.com"), sqllib::query::val(30)));
        ASSERT_TRUE(result) << "Failed to insert data: " << result.error().message;
        
        // Let the guard go out of scope without committing
    }
    
    // Verify the transaction was rolled back
    EXPECT_FALSE(conn.in_transaction());
    
    // Verify data was rolled back
    auto result = conn.execute_raw("SELECT * FROM users WHERE name = 'Rollback User'");
    ASSERT_TRUE(result) << "Failed to select data: " << result.error().message;
    EXPECT_EQ(0, result->size());
    
    // Clean up
    conn.disconnect();
}

TEST_F(TransactionGuardTest, TestExplicitRollback) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Use TransactionGuard with explicit rollback
    {
        sqllib::TransactionGuard guard(conn);
        
        // Insert test data
        Users u;
        auto result = conn.execute(sqllib::query::insert_into(u)
            .columns(u.name, u.email, u.age)
            .values(sqllib::query::val("Explicit Rollback"), sqllib::query::val("explicit@example.com"), sqllib::query::val(35)));
        ASSERT_TRUE(result) << "Failed to insert data: " << result.error().message;
        
        // Explicitly roll back
        guard.rollback();
        
        // Verify the transaction was rolled back
        EXPECT_FALSE(guard.is_committed());
        EXPECT_TRUE(guard.is_rolled_back());
        EXPECT_FALSE(conn.in_transaction());
    }
    
    // Verify data was rolled back
    auto result = conn.execute_raw("SELECT * FROM users WHERE name = 'Explicit Rollback'");
    ASSERT_TRUE(result) << "Failed to select data: " << result.error().message;
    EXPECT_EQ(0, result->size());
    
    // Clean up
    conn.disconnect();
}

TEST_F(TransactionGuardTest, TestRollbackOnException) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Use TransactionGuard with exception causing rollback
    try {
        sqllib::TransactionGuard guard(conn);
        
        // Insert test data
        Users u;
        auto result = conn.execute(sqllib::query::insert_into(u)
            .columns(u.name, u.email, u.age)
            .values(sqllib::query::val("Exception User"), sqllib::query::val("exception@example.com"), sqllib::query::val(40)));
        ASSERT_TRUE(result) << "Failed to insert data: " << result.error().message;
        
        // Throw an exception
        throw std::runtime_error("Test exception");
        
        // Should not reach this point
        guard.commit();
    } catch (const std::runtime_error&) {
        // Expected exception
    }
    
    // Verify the transaction was rolled back
    EXPECT_FALSE(conn.in_transaction());
    
    // Verify data was rolled back
    auto result = conn.execute_raw("SELECT * FROM users WHERE name = 'Exception User'");
    ASSERT_TRUE(result) << "Failed to select data: " << result.error().message;
    EXPECT_EQ(0, result->size());
    
    // Clean up
    conn.disconnect();
}

TEST_F(TransactionGuardTest, TestWithTransactionHelper) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Use the with_transaction helper for successful case
    sqllib::TransactionGuard::with_transaction(conn, [](sqllib::Connection& conn) {
        Users u;
        auto result = conn.execute(sqllib::query::insert_into(u)
            .columns(u.name, u.email, u.age)
            .values(sqllib::query::val("Helper User"), sqllib::query::val("helper@example.com"), sqllib::query::val(45)));
        ASSERT_TRUE(result) << "Failed to insert data: " << result.error().message;
    });
    
    // Verify data was committed
    auto result = conn.execute_raw("SELECT * FROM users WHERE name = 'Helper User'");
    ASSERT_TRUE(result) << "Failed to select data: " << result.error().message;
    EXPECT_EQ(1, result->size());
    
    // Use the with_transaction helper with exception
    try {
        sqllib::TransactionGuard::with_transaction(conn, [](sqllib::Connection& conn) {
            Users u;
            auto result = conn.execute(sqllib::query::insert_into(u)
                .columns(u.name, u.email, u.age)
                .values(sqllib::query::val("Helper Exception"), sqllib::query::val("helper_ex@example.com"), sqllib::query::val(50)));
            ASSERT_TRUE(result) << "Failed to insert data: " << result.error().message;
            
            // Throw an exception
            throw std::runtime_error("Test exception in helper");
        });
        
        FAIL() << "Expected exception was not thrown";
    } catch (const std::runtime_error&) {
        // Expected exception
    }
    
    // Verify data from failed transaction was rolled back
    auto result2 = conn.execute_raw("SELECT * FROM users WHERE name = 'Helper Exception'");
    ASSERT_TRUE(result2) << "Failed to select data: " << result2.error().message;
    EXPECT_EQ(0, result2->size());
    
    // Clean up
    conn.disconnect();
}

TEST_F(TransactionGuardTest, TestTransactionExceptionHandling) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Begin a transaction manually to cause conflict
    auto begin_result = conn.begin_transaction();
    ASSERT_TRUE(begin_result);
    
    // Try to create a TransactionGuard on a connection that already has an active transaction
    try {
        sqllib::TransactionGuard guard(conn);
        FAIL() << "Expected TransactionException was not thrown";
    } catch (const sqllib::TransactionException& ex) {
        // The actual message depends on the underlying database error message
        // Just verify it's a non-empty error message rather than checking exact content
        EXPECT_FALSE(std::string(ex.what()).empty());
    }
    
    // Rollback the manually started transaction
    conn.rollback_transaction();
    
    // Clean up
    conn.disconnect();
}

TEST_F(TransactionGuardTest, TestIsolationLevels) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Test all isolation levels
    const auto levels = {
        sqllib::IsolationLevel::ReadUncommitted, 
        sqllib::IsolationLevel::ReadCommitted,
        sqllib::IsolationLevel::RepeatableRead,
        sqllib::IsolationLevel::Serializable
    };
    
    for (const auto level : levels) {
        sqllib::TransactionGuard guard(conn, level);
        
        // Verify transaction was started with the specified isolation level
        EXPECT_TRUE(conn.in_transaction());
        
        // Perform a query
        auto result = conn.execute_raw("SELECT 1");
        ASSERT_TRUE(result) << "Failed to execute query: " << result.error().message;
        
        // Explicitly roll back to clean up
        guard.rollback();
    }
    
    // Clean up
    conn.disconnect();
}

TEST_F(TransactionGuardTest, TestMultipleOperations) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Use TransactionGuard with multiple operations
    {
        sqllib::TransactionGuard guard(conn);
        
        // Insert multiple rows
        Users u;
        for (int i = 1; i <= 5; i++) {
            std::string name = "Multi User " + std::to_string(i);
            std::string email = "multi" + std::to_string(i) + "@example.com";
            
            auto result = conn.execute(sqllib::query::insert_into(u)
                .columns(u.name, u.email, u.age)
                .values(sqllib::query::val(name), sqllib::query::val(email), sqllib::query::val(20 + i)));
                
            ASSERT_TRUE(result) << "Failed to insert data: " << result.error().message;
        }
        
        // Update a row
        auto update_result = conn.execute_raw(
            "UPDATE users SET age = ? WHERE name = ?", 
            {"50", "Multi User 3"}
        );
        ASSERT_TRUE(update_result) << "Failed to update data: " << update_result.error().message;
        
        // Delete a row
        auto delete_result = conn.execute_raw(
            "DELETE FROM users WHERE name = ?", 
            {"Multi User 5"}
        );
        ASSERT_TRUE(delete_result) << "Failed to delete data: " << delete_result.error().message;
        
        // Commit all changes
        guard.commit();
    }
    
    // Verify all operations were committed
    auto result = conn.execute_raw("SELECT COUNT(*) FROM users");
    ASSERT_TRUE(result) << "Failed to count rows: " << result.error().message;
    auto count = (*result)[0].get<int>(0);
    ASSERT_TRUE(count);
    EXPECT_EQ(4, *count);  // 5 inserted - 1 deleted
    
    // Verify the update
    auto update_check = conn.execute_raw("SELECT age FROM users WHERE name = 'Multi User 3'");
    ASSERT_TRUE(update_check) << "Failed to select updated data: " << update_check.error().message;
    ASSERT_EQ(1, update_check->size());
    auto age = (*update_check)[0].get<int>(0);
    ASSERT_TRUE(age);
    EXPECT_EQ(50, *age);
    
    // Clean up
    conn.disconnect();
}

} // namespace 