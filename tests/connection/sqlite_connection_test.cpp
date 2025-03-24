#include <gtest/gtest.h>

#include <sqllib/connection.hpp>
#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>
#include <sqllib/results.hpp>

#include <filesystem>
#include <fstream>

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

class SQLiteConnectionTest : public ::testing::Test {
protected:
    std::string db_path = "test_db.sqlite";
    
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
    
    // Helper to insert test data
    void insert_test_data(sqllib::Connection& conn) {
        Users u;
        auto result = conn.execute(sqllib::query::insert_into(u)
            .columns(u.name, u.email, u.age)
            .values(sqllib::query::val("Alice"), sqllib::query::val("alice@example.com"), sqllib::query::val(30)));
        ASSERT_TRUE(result) << "Failed to insert test data: " << result.error().message;
        
        // Insert test user 2
        auto result2 = conn.execute(sqllib::query::insert_into(u)
            .columns(u.name, u.email, u.age)
            .values(sqllib::query::val("Bob"), sqllib::query::val("bob@example.com"), sqllib::query::val(25)));
        ASSERT_TRUE(result2) << "Failed to insert test data 2: " << result2.error().message;
        
        // Insert test user 3
        auto result3 = conn.execute(sqllib::query::insert_into(u)
            .columns(u.name, u.email, u.age)
            .values(sqllib::query::val("Charlie"), sqllib::query::val("charlie@example.com"), sqllib::query::val(35)));
        ASSERT_TRUE(result3) << "Failed to insert test data 3: " << result3.error().message;
    }
};

TEST_F(SQLiteConnectionTest, TestConnection) {
    sqllib::SQLiteConnection conn(db_path);
    
    // Test initial state
    EXPECT_FALSE(conn.is_connected());
    
    // Test connect
    auto connect_result = conn.connect();
    ASSERT_TRUE(connect_result) << "Connect failed: " << connect_result.error().message;
    EXPECT_TRUE(conn.is_connected());
    
    // Test disconnect
    auto disconnect_result = conn.disconnect();
    ASSERT_TRUE(disconnect_result) << "Disconnect failed: " << disconnect_result.error().message;
    EXPECT_FALSE(conn.is_connected());
    
    // Test connecting twice
    ASSERT_TRUE(conn.connect());
    ASSERT_TRUE(conn.connect()) << "Second connect should be a no-op and succeed";
    EXPECT_TRUE(conn.is_connected());
    
    // Clean up
    conn.disconnect();
}

TEST_F(SQLiteConnectionTest, TestExecuteRawQuery) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    insert_test_data(conn);
    
    // Test executing a select query
    auto result = conn.execute_raw("SELECT * FROM users ORDER BY id");
    ASSERT_TRUE(result) << "Query failed: " << result.error().message;
    
    // Check result structure
    ASSERT_EQ(3, result->size());
    ASSERT_EQ(4, result->column_count());
    
    // Check column names
    ASSERT_EQ("id", result->column_name(0));
    ASSERT_EQ("name", result->column_name(1));
    ASSERT_EQ("email", result->column_name(2));
    ASSERT_EQ("age", result->column_name(3));
    
    // Check first row
    const auto& row1 = (*result)[0];
    auto id1 = row1.get<int>("id");
    auto name1 = row1.get<std::string>("name");
    auto email1 = row1.get<std::string>("email");
    auto age1 = row1.get<int>("age");
    
    ASSERT_TRUE(id1);
    ASSERT_TRUE(name1);
    ASSERT_TRUE(email1);
    ASSERT_TRUE(age1);
    
    EXPECT_EQ(1, *id1);
    EXPECT_EQ("Alice", *name1);
    EXPECT_EQ("alice@example.com", *email1);
    EXPECT_EQ(30, *age1);
    
    // Clean up
    conn.disconnect();
}

TEST_F(SQLiteConnectionTest, TestExecuteQueryWithParams) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    insert_test_data(conn);
    
    // Test executing a parameterized query
    auto result = conn.execute_raw("SELECT * FROM users WHERE age > ?", {"28"});
    ASSERT_TRUE(result) << "Query failed: " << result.error().message;
    
    // Check result structure
    ASSERT_EQ(2, result->size());
    
    // Check data
    bool found_alice = false;
    bool found_charlie = false;
    
    for (const auto& row : *result) {
        auto name = row.get<std::string>("name");
        ASSERT_TRUE(name);
        
        if (*name == "Alice") {
            found_alice = true;
            auto age = row.get<int>("age");
            ASSERT_TRUE(age);
            EXPECT_EQ(30, *age);
        } else if (*name == "Charlie") {
            found_charlie = true;
            auto age = row.get<int>("age");
            ASSERT_TRUE(age);
            EXPECT_EQ(35, *age);
        }
    }
    
    EXPECT_TRUE(found_alice);
    EXPECT_TRUE(found_charlie);
    
    // Clean up
    conn.disconnect();
}

TEST_F(SQLiteConnectionTest, TestErrorHandling) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Test syntax error
    auto result1 = conn.execute_raw("SELECT * FORM users"); // Deliberate typo
    EXPECT_FALSE(result1);
    EXPECT_NE("", result1.error().message);
    
    // Test table doesn't exist
    auto result2 = conn.execute_raw("SELECT * FROM nonexistent_table");
    EXPECT_FALSE(result2);
    EXPECT_NE("", result2.error().message);
    
    // Test parameter count mismatch
    auto result3 = conn.execute_raw("SELECT * FROM sqlite_master WHERE type = ?", {"table", "extra_param"});
    EXPECT_FALSE(result3);
    EXPECT_NE("", result3.error().message);
    
    // Test executing without connecting
    sqllib::SQLiteConnection conn2(db_path);
    auto result4 = conn2.execute_raw("SELECT 1");
    EXPECT_FALSE(result4);
    EXPECT_NE("", result4.error().message);
    
    // Clean up
    conn.disconnect();
}

TEST_F(SQLiteConnectionTest, TestMoveOperations) {
    sqllib::SQLiteConnection conn1(db_path);
    ASSERT_TRUE(conn1.connect());
    
    // Create test table
    create_test_table(conn1);
    
    // Test move constructor
    sqllib::SQLiteConnection conn2(std::move(conn1));
    EXPECT_TRUE(conn2.is_connected());
    
    // Original connection should be in a disconnected state
    EXPECT_FALSE(conn1.is_connected());
    
    // New connection should work
    auto result1 = conn2.execute_raw("SELECT 1");
    ASSERT_TRUE(result1);
    
    // Test move assignment
    sqllib::SQLiteConnection conn3(":memory:");
    conn3 = std::move(conn2);
    EXPECT_TRUE(conn3.is_connected());
    EXPECT_FALSE(conn2.is_connected());
    
    // New connection should work and have access to the table
    auto result2 = conn3.execute_raw("SELECT COUNT(*) FROM users");
    ASSERT_TRUE(result2);
    auto count = (*result2)[0].get<int>(0);
    ASSERT_TRUE(count);
    EXPECT_EQ(0, *count);
    
    // Clean up
    conn3.disconnect();
}

TEST_F(SQLiteConnectionTest, TestQueryObjectExecution) {
    sqllib::SQLiteConnection conn(db_path);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Insert test data
    insert_test_data(conn);
    
    // Create query using the query builder
    Users u;
    auto query = sqllib::query::select(u.id, u.name, u.email)
        .from(u)
        .where(sqllib::query::to_expr(u.age) > sqllib::query::val(28))
        .order_by(sqllib::query::column_ref(u.name));
    
    // Execute the query
    auto result = conn.execute(query);
    ASSERT_TRUE(result) << "Query failed: " << result.error().message;
    
    // Check result
    ASSERT_EQ(2, result->size());
    
    // Check data in order (Alice, Charlie)
    const auto& row1 = (*result)[0];
    auto name1 = row1.get<std::string>("name");
    ASSERT_TRUE(name1);
    EXPECT_EQ("Alice", *name1);
    
    const auto& row2 = (*result)[1];
    auto name2 = row2.get<std::string>("name");
    ASSERT_TRUE(name2);
    EXPECT_EQ("Charlie", *name2);
    
    // Clean up
    conn.disconnect();
}

} // namespace 