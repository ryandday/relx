#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/schema.hpp>

using namespace relx::schema;

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

// Test table with optional (nullable) columns
struct [[=relx::table("customers")]] Customer {
  int id;
  std::string customer_name;
  std::optional<std::string> email;                          // Nullable email
  std::optional<std::string> phone;                          // Nullable phone
  std::optional<int> age;                                    // Nullable age
  [[=relx::default_value<0>{}]] int vip_level;               // Non-nullable with default
  [[=relx::null_default{}]] std::optional<std::string> notes;  // Nullable with NULL default
};

// clang-format on

TEST(OptionalColumnTest, OptionalProperties) {
  // Test regular column (non-nullable)
  column<Customer, "id", int> id_col;
  EXPECT_FALSE(id_col.nullable);
  EXPECT_EQ(id_col.sql_definition(), "id INTEGER NOT NULL");

  // Test std::optional column (nullable)
  column<Customer, "email", std::optional<std::string>> email_col;
  EXPECT_TRUE(email_col.nullable);
  EXPECT_EQ(email_col.sql_definition(), "email TEXT");

  // Test std::optional with default
  column<Customer, "count", std::optional<int>, default_value<42>> count_col;
  EXPECT_TRUE(count_col.nullable);
  EXPECT_TRUE(count_col.sql_definition().find("DEFAULT 42") != std::string::npos);
  EXPECT_EQ(count_col.sql_definition(), "count INTEGER DEFAULT 42");

  // Test std::optional with NULL default
  column<Customer, "inactive", std::optional<bool>, null_default> inactive_col;
  EXPECT_TRUE(inactive_col.nullable);
  EXPECT_EQ(inactive_col.sql_definition(), "inactive BOOLEAN DEFAULT NULL");
}

TEST(OptionalColumnTest, ValueConversion) {
  // Test conversion of non-NULL values
  column<Customer, "email", std::optional<std::string>> email_col;

  // Converting value to SQL string
  std::optional<std::string> email_value = "test@example.com";
  EXPECT_EQ(email_col.to_sql_string(email_value), "'test@example.com'");

  // Converting from raw protocol text
  auto parsed_email = email_col.from_sql_string("test@example.com");
  ASSERT_TRUE(parsed_email.has_value());
  EXPECT_EQ(*parsed_email, "test@example.com");

  // Test conversion of NULL values
  std::optional<std::string> null_email;
  EXPECT_EQ(email_col.to_sql_string(null_email), "NULL");

  // NULL is out-of-band: the text "NULL" is real data, not SQL NULL
  auto parsed_null = email_col.from_sql_string("NULL");
  ASSERT_TRUE(parsed_null.has_value());
  EXPECT_EQ(*parsed_null, "NULL");
}

TEST(OptionalColumnTest, TableWithOptionalColumns) {
  // Generate CREATE TABLE SQL with optional columns
  std::string create_sql = create_table(relx::t<Customer>).to_sql();

  // Validate SQL contains optional and non-optional columns
  EXPECT_TRUE(create_sql.find("id INTEGER NOT NULL") != std::string::npos);
  EXPECT_TRUE(create_sql.find("customer_name TEXT NOT NULL") != std::string::npos);

  // Optional columns should not have NOT NULL
  EXPECT_TRUE(create_sql.find("email TEXT") != std::string::npos);
  EXPECT_FALSE(create_sql.find("email TEXT NOT NULL") != std::string::npos);

  EXPECT_TRUE(create_sql.find("phone TEXT") != std::string::npos);
  EXPECT_FALSE(create_sql.find("phone TEXT NOT NULL") != std::string::npos);

  EXPECT_TRUE(create_sql.find("age INTEGER") != std::string::npos);
  EXPECT_FALSE(create_sql.find("age INTEGER NOT NULL") != std::string::npos);

  // Default values should be present
  EXPECT_TRUE(create_sql.find("vip_level INTEGER NOT NULL DEFAULT 0") != std::string::npos);
  EXPECT_TRUE(create_sql.find("notes TEXT DEFAULT NULL") != std::string::npos);
}

}  // namespace
