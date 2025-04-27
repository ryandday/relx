#include <gtest/gtest.h>
#include "relx/schema/autoincrement.hpp"
#include "relx/schema/column.hpp"
#include "relx/schema/table.hpp"

using namespace relx::schema;

// Test table with autoincrement primary key for SQLite
struct SqliteUserTable {
    static constexpr auto table_name = "users";
    
    sqlite_autoincrement<"id"> id;
    column<"name", std::string> name;
    column<"email", std::string> email;
};

// Test table with autoincrement primary key for PostgreSQL
struct PostgresUserTable {
    static constexpr auto table_name = "users";
    
    pg_serial<"id"> id;
    column<"name", std::string> name;
    column<"email", std::string> email;
};

// Test table with autoincrement primary key for MySQL
struct MySQLUserTable {
    static constexpr auto table_name = "users";
    
    mysql_auto_increment<"id"> id;
    column<"name", std::string> name;
    column<"email", std::string> email;
};

// Test table with generic autoincrement primary key
struct GenericUserTable {
    static constexpr auto table_name = "users";
    
    autoincrement<"id"> id;
    column<"name", std::string> name;
    column<"email", std::string> email;
};

// Test table with bigint for PostgreSQL
struct BigIdTable {
    static constexpr auto table_name = "big_ids";
    pg_serial<"id", long long> id;
};

// Test table with custom type for PostgreSQL
struct CustomIdTable {
    static constexpr auto table_name = "custom_ids";
    pg_serial<"id", float> id; // Not a typical ID type, but testing the fallback
};

TEST(AutoincrementTest, SQLiteDialect) {
    SqliteUserTable table;
    std::string sql = create_table(table);
    
    // Verify that the ID column is defined with SQLite-specific AUTOINCREMENT
    EXPECT_TRUE(sql.find("id INTEGER PRIMARY KEY AUTOINCREMENT") != std::string::npos);
    
    // Verify full table creation SQL
    std::string expected = "CREATE TABLE IF NOT EXISTS users (\n"
                           "id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                           "name TEXT NOT NULL,\n"
                           "email TEXT NOT NULL\n"
                           ");";
    EXPECT_EQ(sql, expected);
}

TEST(AutoincrementTest, PostgreSQLDialect) {
    PostgresUserTable table;
    std::string sql = create_table(table);
    
    // Verify that the ID column is defined with PostgreSQL-specific SERIAL
    EXPECT_TRUE(sql.find("id SERIAL") != std::string::npos);
    
    // Verify full table creation SQL
    std::string expected = "CREATE TABLE IF NOT EXISTS users (\n"
                           "id SERIAL,\n"
                           "name TEXT NOT NULL,\n"
                           "email TEXT NOT NULL\n"
                           ");";
    EXPECT_EQ(sql, expected);
}

TEST(AutoincrementTest, MySQLDialect) {
    MySQLUserTable table;
    std::string sql = create_table(table);
    
    // Verify that the ID column is defined with MySQL-specific AUTO_INCREMENT
    EXPECT_TRUE(sql.find("id INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY") != std::string::npos);
    
    // Verify full table creation SQL
    std::string expected = "CREATE TABLE IF NOT EXISTS users (\n"
                           "id INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY,\n"
                           "name TEXT NOT NULL,\n"
                           "email TEXT NOT NULL\n"
                           ");";
    EXPECT_EQ(sql, expected);
}

TEST(AutoincrementTest, GenericDialect) {
    GenericUserTable table;
    std::string sql = create_table(table);
    
    // Verify that the ID column is defined with generic AUTO_INCREMENT
    EXPECT_TRUE(sql.find("id INTEGER PRIMARY KEY AUTO_INCREMENT") != std::string::npos);
    
    // Verify full table creation SQL
    std::string expected = "CREATE TABLE IF NOT EXISTS users (\n"
                           "id INTEGER PRIMARY KEY AUTO_INCREMENT,\n"
                           "name TEXT NOT NULL,\n"
                           "email TEXT NOT NULL\n"
                           ");";
    EXPECT_EQ(sql, expected);
}

TEST(AutoincrementTest, BigSerial) {
    BigIdTable table;
    std::string sql = create_table(table);
    
    // Verify that PostgreSQL uses BIGSERIAL for 64-bit integers
    EXPECT_TRUE(sql.find("id BIGSERIAL") != std::string::npos);
}

TEST(AutoincrementTest, CustomType) {
    CustomIdTable table;
    std::string sql = create_table(table);
    
    // Verify that PostgreSQL uses GENERATED ALWAYS AS IDENTITY for non-standard types
    EXPECT_TRUE(sql.find("id REAL GENERATED ALWAYS AS IDENTITY") != std::string::npos);
} 