#include <gtest/gtest.h>
#include <sqllib/schema.hpp>
#include <chrono>
#include <array>
#include <optional>

using namespace sqllib::schema;

// Custom enum type with column traits specialization
enum class UserRole {
    Admin,
    User,
    Guest
};

// Specialization of column_traits for the UserRole enum
template <>
struct sqllib::schema::column_traits<UserRole> {
    static constexpr auto sql_type_name = "TEXT";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const UserRole& role) {
        switch (role) {
            case UserRole::Admin: return "'ADMIN'";
            case UserRole::User:  return "'USER'";
            case UserRole::Guest: return "'GUEST'";
            default:              return "'UNKNOWN'";
        }
    }
    
    static UserRole from_sql_string(const std::string& value) {
        std::string unquoted = value;
        // Remove quotes if present
        if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
            unquoted = value.substr(1, value.size() - 2);
        }
        
        if (unquoted == "ADMIN") return UserRole::Admin;
        if (unquoted == "USER")  return UserRole::User;
        if (unquoted == "GUEST") return UserRole::Guest;
        
        return UserRole::Guest; // Default
    }
};

// Custom UUID-like type
struct UUID {
    std::array<uint8_t, 16> data;
    
    bool operator==(const UUID& other) const {
        return data == other.data;
    }
};

// Specialization of column_traits for UUID
template <>
struct sqllib::schema::column_traits<UUID> {
    static constexpr auto sql_type_name = "BLOB";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const UUID& uuid) {
        // This is a simplified example - in a real app we would convert to hex or base64
        std::string result = "X'";
        char hex[3];
        for (auto byte : uuid.data) {
            snprintf(hex, sizeof(hex), "%02X", byte);
            result += hex;
        }
        result += "'";
        return result;
    }
    
    static UUID from_sql_string(const std::string& value) {
        // This is a simplified example - in a real app we would parse the hex or base64
        UUID result{};
        // Just for testing - not actually parsing
        return result;
    }
};

// A custom timestamp type with column traits
using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

// Specialization of column_traits for Timestamp
template <>
struct sqllib::schema::column_traits<Timestamp> {
    static constexpr auto sql_type_name = "TEXT";
    static constexpr bool nullable = false;
    
    static std::string to_sql_string(const Timestamp& ts) {
        // Convert to ISO format string 
        // This is a simplified version - a real implementation would be more complete
        auto time = std::chrono::system_clock::to_time_t(ts);
        std::tm* tm = std::gmtime(&time);
        
        char buffer[30];
        std::strftime(buffer, sizeof(buffer), "'%Y-%m-%dT%H:%M:%SZ'", tm);
        return buffer;
    }
    
    static Timestamp from_sql_string(const std::string& value) {
        // Parse ISO format - a real implementation would be more complete
        // Here we just return the current time for testing
        return std::chrono::system_clock::now();
    }
};

// Test table with custom column types
struct CustomTypesTable {
    static constexpr auto name = std::string_view("custom_types");
    
    column<"id", int> id;
    column<"role", UserRole> role;
    column<"uuid", UUID> uuid;
    column<"created_at", Timestamp> created_at;
    column<"updated_at", std::optional<Timestamp>> updated_at;
    
    primary_key<&CustomTypesTable::id> pk;
};

TEST(CustomColumnTypesTest, UserRoleType) {
    column<"role", UserRole> role_col;
    
    // Test SQL type
    EXPECT_EQ(std::string_view(role_col.sql_type), "TEXT");
    
    // Test SQL definition
    EXPECT_EQ(role_col.sql_definition(), "role TEXT NOT NULL");
    
    // Test serialization
    EXPECT_EQ(role_col.to_sql_string(UserRole::Admin), "'ADMIN'");
    EXPECT_EQ(role_col.to_sql_string(UserRole::User), "'USER'");
    EXPECT_EQ(role_col.to_sql_string(UserRole::Guest), "'GUEST'");
    
    // Test deserialization
    EXPECT_EQ(role_col.from_sql_string("'ADMIN'"), UserRole::Admin);
    EXPECT_EQ(role_col.from_sql_string("'USER'"), UserRole::User);
    EXPECT_EQ(role_col.from_sql_string("'GUEST'"), UserRole::Guest);
    
    // Test without quotes
    EXPECT_EQ(role_col.from_sql_string("ADMIN"), UserRole::Admin);
}

TEST(CustomColumnTypesTest, UUIDType) {
    column<"uuid", UUID> uuid_col;
    
    // Test SQL type
    EXPECT_EQ(std::string_view(uuid_col.sql_type), "BLOB");
    
    // Test SQL definition
    EXPECT_EQ(uuid_col.sql_definition(), "uuid BLOB NOT NULL");
    
    // Create a test UUID
    UUID test_uuid = {};
    for (size_t i = 0; i < 16; ++i) {
        test_uuid.data[i] = static_cast<uint8_t>(i);
    }
    
    // Test serialization - basic check that it contains the correct format
    std::string sql = uuid_col.to_sql_string(test_uuid);
    EXPECT_TRUE(sql.find("X'") == 0);
    EXPECT_TRUE(sql.find('\'', 2) != std::string::npos);
    
    // Test deserialization - here we just check that it doesn't crash
    UUID parsed = uuid_col.from_sql_string(sql);
}

TEST(CustomColumnTypesTest, TimestampType) {
    column<"created_at", Timestamp> timestamp_col;
    
    // Test SQL type
    EXPECT_EQ(std::string_view(timestamp_col.sql_type), "TEXT");
    
    // Test SQL definition
    EXPECT_EQ(timestamp_col.sql_definition(), "created_at TEXT NOT NULL");
    
    // Get current time for testing
    Timestamp now = std::chrono::system_clock::now();
    
    // Test serialization format - should be ISO format with quotes
    std::string sql = timestamp_col.to_sql_string(now);
    EXPECT_TRUE(sql.front() == '\'');
    EXPECT_TRUE(sql.back() == '\'');
    EXPECT_TRUE(sql.find('T') != std::string::npos);
    EXPECT_TRUE(sql.find('Z') != std::string::npos);
    
    // Test deserialization - here we just check that it doesn't crash
    Timestamp parsed = timestamp_col.from_sql_string(sql);
}

TEST(CustomColumnTypesTest, TableWithCustomTypes) {
    CustomTypesTable table;
    
    // Generate CREATE TABLE SQL
    std::string sql = create_table_sql(table);
    
    // Check that the custom column types are included
    EXPECT_TRUE(sql.find("id INTEGER NOT NULL") != std::string::npos);
    EXPECT_TRUE(sql.find("role TEXT NOT NULL") != std::string::npos);
    EXPECT_TRUE(sql.find("uuid BLOB NOT NULL") != std::string::npos);
    EXPECT_TRUE(sql.find("created_at TEXT NOT NULL") != std::string::npos);
    EXPECT_TRUE(sql.find("updated_at TEXT") != std::string::npos);
    
    // Check primary key constraint
    EXPECT_TRUE(sql.find("PRIMARY KEY (id)") != std::string::npos);
} 