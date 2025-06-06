#include <gtest/gtest.h>

#include <relx/connection/postgresql_connection.hpp>
#include <relx/connection/postgresql_statement.hpp>
#include <string>
#include <memory>

namespace {

class PostgreSQLStatementTest : public ::testing::Test {
protected:
    // Connection string for the Docker container
    std::string conn_string = "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
    
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
        relx::connection::PostgreSQLConnection conn(conn_string);
        auto connect_result = conn.connect();
        if (connect_result) {
            // Drop table if it exists
            conn.execute_raw("DROP TABLE IF EXISTS prepared_test");
            
            // Clean up any prepared statements that might be lingering
            // These are common statement names used in our tests
            const std::vector<std::string> statement_names = {
                "insert_statement", "insert_typed_statement", "statement1", 
                "insert_stmt", "update_stmt", "select_stmt"
            };
            
            // PostgreSQL doesn't support "IF EXISTS" for DEALLOCATE, so we need to check first
            // Get a list of all prepared statements that match our names
            std::string check_sql = "SELECT name FROM pg_prepared_statements";
            auto result = conn.execute_raw(check_sql);
            if (result) {
                for (const auto& row : *result) {
                    auto stmt_name = row.get<std::string>(0);
                    if (stmt_name) {
                        // Check if this statement is one of ours
                        if (std::find(statement_names.begin(), statement_names.end(), *stmt_name) != statement_names.end()) {
                            try {
                                conn.execute_raw("DEALLOCATE " + *stmt_name);
                            } catch (...) {
                                // Ignore any errors
                            }
                        }
                    }
                }
            }
            
            conn.disconnect();
        }
    }
    
    // Helper to create the test table
    void create_test_table(relx::connection::PostgreSQLConnection& conn) {
        // Clean up any lingering prepared statements first
        const std::vector<std::string> statement_names = {
            "insert_statement", "insert_typed_statement", "statement1", 
            "insert_stmt", "update_stmt", "select_stmt"
        };
        
        // PostgreSQL doesn't support "IF EXISTS" for DEALLOCATE, so we need to check first
        // Get a list of all prepared statements that match our names
        std::string check_sql = "SELECT name FROM pg_prepared_statements";
        auto result = conn.execute_raw(check_sql);
        if (result) {
            for (const auto& row : *result) {
                auto stmt_name = row.get<std::string>(0);
                if (stmt_name) {
                    // Check if this statement is one of ours
                    if (std::find(statement_names.begin(), statement_names.end(), *stmt_name) != statement_names.end()) {
                        try {
                            conn.execute_raw("DEALLOCATE " + *stmt_name);
                        } catch (...) {
                            // Ignore any errors
                        }
                    }
                }
            }
        }
        
        std::string create_table_sql = R"(
            CREATE TABLE IF NOT EXISTS prepared_test (
                id SERIAL PRIMARY KEY,
                name TEXT NOT NULL,
                value INTEGER NOT NULL
            )
        )";
        auto create_result = conn.execute_raw(create_table_sql);
        ASSERT_TRUE(create_result) << "Failed to create table: " << create_result.error().message;
    }
};

TEST_F(PostgreSQLStatementTest, TestBasicPreparedStatement) {
    relx::connection::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Create a prepared statement
    auto stmt = conn.prepare_statement(
        "insert_statement",
        "INSERT INTO prepared_test (name, value) VALUES ($1, $2)",
        2 // 2 parameters
    );
    
    // Execute the prepared statement multiple times
    auto result1 = stmt->execute({"Item 1", "100"});
    ASSERT_TRUE(result1) << "Failed to execute prepared statement: " << result1.error().message;
    
    auto result2 = stmt->execute({"Item 2", "200"});
    ASSERT_TRUE(result2) << "Failed to execute prepared statement: " << result2.error().message;
    
    auto result3 = stmt->execute({"Item 3", "300"});
    ASSERT_TRUE(result3) << "Failed to execute prepared statement: " << result3.error().message;
    
    // Verify the data was inserted correctly
    auto select_result = conn.execute_raw("SELECT * FROM prepared_test ORDER BY id");
    ASSERT_TRUE(select_result) << "Failed to select data: " << select_result.error().message;
    ASSERT_EQ(3, select_result->size());
    
    // Check first row
    const auto& row1 = (*select_result)[0];
    auto name1 = row1.get<std::string>("name");
    auto value1 = row1.get<int>("value");
    
    ASSERT_TRUE(name1);
    ASSERT_TRUE(value1);
    
    EXPECT_EQ("Item 1", *name1);
    EXPECT_EQ(100, *value1);
    
    // Check second row
    const auto& row2 = (*select_result)[1];
    auto name2 = row2.get<std::string>("name");
    auto value2 = row2.get<int>("value");
    
    ASSERT_TRUE(name2);
    ASSERT_TRUE(value2);
    
    EXPECT_EQ("Item 2", *name2);
    EXPECT_EQ(200, *value2);
    
    // Check third row
    const auto& row3 = (*select_result)[2];
    auto name3 = row3.get<std::string>("name");
    auto value3 = row3.get<int>("value");
    
    ASSERT_TRUE(name3);
    ASSERT_TRUE(value3);
    
    EXPECT_EQ("Item 3", *name3);
    EXPECT_EQ(300, *value3);
    
    // Explicitly deallocate the statement
    stmt.reset(); // This will call the destructor and deallocate
    
    // Clean up
    conn.disconnect();
}

TEST_F(PostgreSQLStatementTest, TestTypedPreparedStatement) {
    relx::connection::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Create a prepared statement
    auto stmt = conn.prepare_statement(
        "insert_typed_statement",
        "INSERT INTO prepared_test (name, value) VALUES ($1, $2)",
        2 // 2 parameters
    );
    
    // Execute the prepared statement with typed parameters
    auto result1 = stmt->execute_typed("Item A", 111);
    ASSERT_TRUE(result1) << "Failed to execute prepared statement: " << result1.error().message;
    
    auto result2 = stmt->execute_typed("Item B", 222);
    ASSERT_TRUE(result2) << "Failed to execute prepared statement: " << result2.error().message;
    
    // Verify the data was inserted correctly
    auto select_result = conn.execute_raw("SELECT * FROM prepared_test ORDER BY name");
    ASSERT_TRUE(select_result) << "Failed to select data: " << select_result.error().message;
    ASSERT_EQ(2, select_result->size());
    
    // Check first row
    const auto& row1 = (*select_result)[0];
    auto name1 = row1.get<std::string>("name");
    auto value1 = row1.get<int>("value");
    
    ASSERT_TRUE(name1);
    ASSERT_TRUE(value1);
    
    EXPECT_EQ("Item A", *name1);
    EXPECT_EQ(111, *value1);
    
    // Check second row
    const auto& row2 = (*select_result)[1];
    auto name2 = row2.get<std::string>("name");
    auto value2 = row2.get<int>("value");
    
    ASSERT_TRUE(name2);
    ASSERT_TRUE(value2);
    
    EXPECT_EQ("Item B", *name2);
    EXPECT_EQ(222, *value2);
    
    // Explicitly deallocate the statement
    stmt.reset(); // This will call the destructor and deallocate
    
    // Clean up
    conn.disconnect();
}

TEST_F(PostgreSQLStatementTest, TestStatementLifecycle) {
    relx::connection::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Scope for the first statement
    {
        // Create a prepared statement
        auto stmt1 = conn.prepare_statement(
            "statement1",
            "INSERT INTO prepared_test (name, value) VALUES ($1, $2)",
            2
        );
        
        // Execute it
        auto result = stmt1->execute({"Lifecycle Test", "999"});
        ASSERT_TRUE(result) << "Failed to execute prepared statement: " << result.error().message;
        
        // Statement should be automatically deallocated when it goes out of scope
    }
    
    // Create another statement with the same name, which should succeed
    // since the previous one should have been deallocated
    auto stmt2 = conn.prepare_statement(
        "statement1", // Same name as before
        "SELECT * FROM prepared_test WHERE value = $1",
        1
    );
    
    // Execute the new statement
    auto result = stmt2->execute({"999"});
    ASSERT_TRUE(result) << "Failed to execute new prepared statement: " << result.error().message;
    ASSERT_EQ(1, result->size());
    
    // Verify the data
    const auto& row = (*result)[0];
    auto name = row.get<std::string>("name");
    auto value = row.get<int>("value");
    
    ASSERT_TRUE(name);
    ASSERT_TRUE(value);
    
    EXPECT_EQ("Lifecycle Test", *name);
    EXPECT_EQ(999, *value);
    
    // Explicitly deallocate the statement
    stmt2.reset();
    
    // Clean up
    conn.disconnect();
}

TEST_F(PostgreSQLStatementTest, TestMultipleStatements) {
    relx::connection::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Create multiple prepared statements at once
    auto insert_stmt = conn.prepare_statement(
        "insert_stmt",
        "INSERT INTO prepared_test (name, value) VALUES ($1, $2)",
        2
    );
    
    auto update_stmt = conn.prepare_statement(
        "update_stmt",
        "UPDATE prepared_test SET value = $1 WHERE name = $2",
        2
    );
    
    auto select_stmt = conn.prepare_statement(
        "select_stmt",
        "SELECT * FROM prepared_test WHERE value > $1 ORDER BY value",
        1
    );
    
    // Insert some data
    ASSERT_TRUE(insert_stmt->execute({"Alpha", "100"}));
    ASSERT_TRUE(insert_stmt->execute({"Beta", "200"}));
    ASSERT_TRUE(insert_stmt->execute({"Gamma", "300"}));
    
    // Update some data
    ASSERT_TRUE(update_stmt->execute({"150", "Alpha"}));
    ASSERT_TRUE(update_stmt->execute({"250", "Beta"}));
    
    // Select data with the prepared statement
    auto result = select_stmt->execute({"200"});
    ASSERT_TRUE(result) << "Failed to execute select statement: " << result.error().message;
    ASSERT_EQ(2, result->size());
    
    // Check first row
    const auto& row1 = (*result)[0];
    auto name1 = row1.get<std::string>("name");
    auto value1 = row1.get<int>("value");
    
    ASSERT_TRUE(name1);
    ASSERT_TRUE(value1);
    
    EXPECT_EQ("Beta", *name1);
    EXPECT_EQ(250, *value1);
    
    // Check second row
    const auto& row2 = (*result)[1];
    auto name2 = row2.get<std::string>("name");
    auto value2 = row2.get<int>("value");
    
    ASSERT_TRUE(name2);
    ASSERT_TRUE(value2);
    
    EXPECT_EQ("Gamma", *name2);
    EXPECT_EQ(300, *value2);
    
    // Explicitly deallocate all statements
    insert_stmt.reset();
    update_stmt.reset();
    select_stmt.reset();
    
    // Clean up
    conn.disconnect();
}

TEST_F(PostgreSQLStatementTest, TestMoveSemantics) {
    relx::connection::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    // Create test table
    create_test_table(conn);
    
    // Test move constructor
    {
        auto stmt1 = conn.prepare_statement(
            "move_test1",
            "INSERT INTO prepared_test (name, value) VALUES ($1, $2)",
            2
        );
        
        EXPECT_TRUE(stmt1->is_valid());
        EXPECT_EQ("move_test1", stmt1->name());
        
        // Move construct
        auto stmt2 = std::move(stmt1);
        
        EXPECT_FALSE(stmt1->is_valid()); // Original should be invalid
        EXPECT_TRUE(stmt2->is_valid());  // Moved-to should be valid
        EXPECT_EQ("move_test1", stmt2->name());
        
        // Test that moved-to statement works
        auto result = stmt2->execute({"Move Test", "123"});
        EXPECT_TRUE(result);
        
        stmt2.reset();
    }
    
    // Test move assignment
    {
        auto stmt1 = conn.prepare_statement(
            "move_test2",
            "INSERT INTO prepared_test (name, value) VALUES ($1, $2)",
            2
        );
        
        auto stmt2 = conn.prepare_statement(
            "move_test3", 
            "SELECT * FROM prepared_test WHERE value = $1",
            1
        );
        
        EXPECT_TRUE(stmt1->is_valid());
        EXPECT_TRUE(stmt2->is_valid());
        EXPECT_EQ("move_test2", stmt1->name());
        EXPECT_EQ("move_test3", stmt2->name());
        
        // Move assign
        stmt2 = std::move(stmt1);
        
        EXPECT_FALSE(stmt1->is_valid()); // Original should be invalid
        EXPECT_TRUE(stmt2->is_valid());  // Moved-to should be valid  
        EXPECT_EQ("move_test2", stmt2->name()); // Should have moved-from name
        
        // Test that moved-to statement works
        auto result = stmt2->execute({"Move Assign Test", "456"});
        EXPECT_TRUE(result);
        
        stmt2.reset();
    }
    
    conn.disconnect();
}

TEST_F(PostgreSQLStatementTest, TestInvalidConnection) {
    // Test statement behavior when connection is closed
    relx::connection::PostgreSQLConnection conn(conn_string);
    ASSERT_TRUE(conn.connect());
    
    create_test_table(conn);
    
    auto stmt = conn.prepare_statement(
        "invalid_conn_test",
        "INSERT INTO prepared_test (name, value) VALUES ($1, $2)",
        2
    );
    
    // Statement should work initially
    auto result1 = stmt->execute({"Initial Test", "100"});
    EXPECT_TRUE(result1);
    
    // Disconnect the connection
    conn.disconnect();
    
    // Now statement should fail (though this depends on implementation)
    // The prepared statement may still exist in PostgreSQL but our connection wrapper
    // should detect the disconnected state
    
    stmt.reset(); // This should handle the disconnected state gracefully
    
    // Test with a completely new connection to ensure cleanup worked
    relx::connection::PostgreSQLConnection conn2(conn_string);
    ASSERT_TRUE(conn2.connect());
    
    // Should be able to create statement with same name since old one was cleaned up
    auto stmt2 = conn2.prepare_statement(
        "invalid_conn_test",
        "SELECT * FROM prepared_test WHERE value = $1", 
        1
    );
    
    EXPECT_TRUE(stmt2->is_valid());
    stmt2.reset();
    conn2.disconnect();
}

} // namespace 