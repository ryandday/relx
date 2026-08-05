#include <iostream>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/migrations.hpp>
#include <relx/schema.hpp>

using namespace relx;

namespace {

// Test table definitions
//
// Single-column UNIQUEs here use the struct-level composite_unique spelling; the
// differ also surfaces inline [[=relx::ann::unique]] as the same table-level
// constraint (see InlineUniqueDiffsAsConstraintNotColumnRebuild below).

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

// Version 1 of Users table
struct [[=relx::table("users"), =relx::ann::composite_unique("email")]] UsersV1 {
  [[=relx::ann::pk]] int id;
  std::string name;
  std::string email;
};

// Version 2 of Users table - with additional columns
struct [[=relx::table("users"), =relx::ann::composite_unique("email")]] UsersV2 {
  [[=relx::ann::pk]] int id;
  std::string name;
  std::string email;
  std::optional<int> age;  // New nullable column
  [[=relx::string_default<"CURRENT_TIMESTAMP", true>{}]]
  std::string created_at;  // New column with default
};

// Version 3 of Users table - removed a column
struct [[=relx::table("users")]] UsersV3 {
  [[=relx::ann::pk]] int id;
  std::string name;
  std::optional<int> age;
  [[=relx::string_default<"CURRENT_TIMESTAMP", true>{}]] std::string created_at;

  // Note: email column and unique constraint removed
};

// Simple table for creation/deletion tests
struct [[=relx::table("simple_table")]] SimpleTable {
  [[=relx::ann::pk]] int id;
  std::string data;
};

// clang-format on

inline constexpr auto users_v1 = relx::t<UsersV1>;
inline constexpr auto users_v2 = relx::t<UsersV2>;
inline constexpr auto users_v3 = relx::t<UsersV3>;
inline constexpr auto simple_table = relx::t<SimpleTable>;

// The CREATE TABLE both the create and the drop migration round-trip through
static const std::string kSimpleTableDdl = "CREATE TABLE simple_table (\n"
                                           "id INTEGER NOT NULL PRIMARY KEY,\n"
                                           "data TEXT NOT NULL\n"
                                           ");";

TEST(MigrationsTest, ExtractTableMetadata) {
  auto metadata_result = migrations::extract_table_metadata(users_v1);

  ASSERT_TRUE(metadata_result) << "Failed to extract metadata: "
                               << metadata_result.error().format();
  const auto& metadata = *metadata_result;

  EXPECT_EQ(metadata.table_name, "users");
  EXPECT_EQ(metadata.columns.size(), 3);

  // Check that columns are properly extracted
  ASSERT_TRUE(metadata.columns.contains("id"));
  ASSERT_TRUE(metadata.columns.contains("name"));
  ASSERT_TRUE(metadata.columns.contains("email"));

  // Check column properties
  EXPECT_EQ(metadata.columns.at("id").name, "id");
  EXPECT_FALSE(metadata.columns.at("id").nullable);
  EXPECT_EQ(metadata.columns.at("name").name, "name");
  EXPECT_EQ(metadata.columns.at("email").name, "email");

  // Check that the struct-level constraint is extracted
  ASSERT_EQ(metadata.constraints.size(), 1);
  EXPECT_EQ(metadata.constraints.at("users_unique_0").type, "UNIQUE");
  EXPECT_EQ(metadata.constraints.at("users_unique_0").sql_definition, "UNIQUE (email)");
}

TEST(MigrationsTest, GenerateAddColumnMigration) {
  auto migration_result = relx::migrations::generate_migration(users_v1, users_v2);

  ASSERT_TRUE(migration_result) << "Failed to generate migration: "
                                << migration_result.error().format();
  const auto& migration = *migration_result;

  EXPECT_FALSE(migration.empty());
  EXPECT_EQ(migration.size(), 2);  // Should have 2 new columns

  auto forward_result = migration.forward_sql();
  ASSERT_TRUE(forward_result) << "Failed to generate forward SQL: "
                              << forward_result.error().format();
  const auto& forward_sqls = *forward_result;
  ASSERT_EQ(forward_sqls.size(), 2);

  // Check exact SQL for adding columns
  EXPECT_EQ(forward_sqls[0], "ALTER TABLE users ADD COLUMN age INTEGER;");
  EXPECT_EQ(forward_sqls[1],
            "ALTER TABLE users ADD COLUMN created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP;");

  // Check rollback SQL - should drop columns in reverse order
  auto rollback_result = migration.rollback_sql();
  ASSERT_TRUE(rollback_result) << "Failed to generate rollback SQL: "
                               << rollback_result.error().format();
  const auto& rollback_sqls = *rollback_result;
  ASSERT_EQ(rollback_sqls.size(), 2);

  EXPECT_EQ(rollback_sqls[0], "ALTER TABLE users DROP COLUMN created_at;");
  EXPECT_EQ(rollback_sqls[1], "ALTER TABLE users DROP COLUMN age;");
}

TEST(MigrationsTest, GenerateDropColumnMigration) {
  auto migration_result = relx::migrations::generate_migration(users_v2, users_v3);

  ASSERT_TRUE(migration_result) << "Failed to generate migration: "
                                << migration_result.error().format();
  const auto& migration = *migration_result;

  EXPECT_FALSE(migration.empty());
  EXPECT_EQ(migration.size(), 2);  // Should drop unique constraint AND column (email)

  auto forward_result = migration.forward_sql();
  ASSERT_TRUE(forward_result) << "Failed to generate forward SQL: "
                              << forward_result.error().format();
  const auto& forward_sqls = *forward_result;
  ASSERT_EQ(forward_sqls.size(), 2);

  // Check that we drop both the constraint and the column
  bool found_drop_constraint = false;
  bool found_drop_column = false;

  for (const auto& sql : forward_sqls) {
    if (sql.find("DROP CONSTRAINT") != std::string::npos) {
      found_drop_constraint = true;
    }
    if (sql.find("DROP COLUMN email") != std::string::npos) {
      found_drop_column = true;
    }
  }

  EXPECT_TRUE(found_drop_constraint);
  EXPECT_TRUE(found_drop_column);

  // Check rollback SQL adds them back
  auto rollback_result = migration.rollback_sql();
  ASSERT_TRUE(rollback_result) << "Failed to generate rollback SQL: "
                               << rollback_result.error().format();
  const auto& rollback_sqls = *rollback_result;
  ASSERT_EQ(rollback_sqls.size(), 2);

  bool found_add_constraint = false;
  bool found_add_column = false;

  for (const auto& sql : rollback_sqls) {
    if (sql.find("UNIQUE (email)") != std::string::npos) {
      found_add_constraint = true;
    }
    if (sql.find("ADD COLUMN email") != std::string::npos) {
      found_add_column = true;
    }
  }

  EXPECT_TRUE(found_add_constraint);
  EXPECT_TRUE(found_add_column);
}

TEST(MigrationsTest, GenerateCreateTableMigration) {
  auto migration_result = relx::migrations::generate_create_table_migration(simple_table);

  ASSERT_TRUE(migration_result) << "Failed to generate create migration: "
                                << migration_result.error().format();
  const auto& migration = *migration_result;

  EXPECT_FALSE(migration.empty());
  EXPECT_EQ(migration.size(), 1);

  auto forward_result = migration.forward_sql();
  ASSERT_TRUE(forward_result) << "Failed to generate forward SQL: "
                              << forward_result.error().format();
  const auto& forward_sqls = *forward_result;
  ASSERT_EQ(forward_sqls.size(), 1);

  // Check exact SQL for table creation
  EXPECT_EQ(forward_sqls[0], kSimpleTableDdl);

  // Check rollback drops the table
  auto rollback_result = migration.rollback_sql();
  ASSERT_TRUE(rollback_result) << "Failed to generate rollback SQL: "
                               << rollback_result.error().format();
  const auto& rollback_sqls = *rollback_result;
  ASSERT_EQ(rollback_sqls.size(), 1);

  EXPECT_EQ(rollback_sqls[0], "DROP TABLE IF EXISTS simple_table;");
}

TEST(MigrationsTest, GenerateDropTableMigration) {
  auto migration_result = migrations::generate_drop_table_migration(simple_table);

  ASSERT_TRUE(migration_result) << "Failed to generate drop migration: "
                                << migration_result.error().format();
  const auto& migration = *migration_result;

  EXPECT_FALSE(migration.empty());
  EXPECT_EQ(migration.size(), 1);

  auto forward_result = migration.forward_sql();
  ASSERT_TRUE(forward_result) << "Failed to generate forward SQL: "
                              << forward_result.error().format();
  const auto& forward_sqls = *forward_result;
  ASSERT_EQ(forward_sqls.size(), 1);

  // Check exact SQL for table drop
  EXPECT_EQ(forward_sqls[0], "DROP TABLE IF EXISTS simple_table;");

  // Check rollback creates the table
  auto rollback_result = migration.rollback_sql();
  ASSERT_TRUE(rollback_result) << "Failed to generate rollback SQL: "
                               << rollback_result.error().format();
  const auto& rollback_sqls = *rollback_result;
  ASSERT_EQ(rollback_sqls.size(), 1);

  EXPECT_EQ(rollback_sqls[0], kSimpleTableDdl);
}

TEST(MigrationsTest, EmptyMigrationForIdenticalTables) {
  auto migration_result = migrations::generate_migration(users_v1, users_v1);

  ASSERT_TRUE(migration_result) << "Failed to generate migration: "
                                << migration_result.error().format();
  const auto& migration = *migration_result;

  EXPECT_TRUE(migration.empty());
  EXPECT_EQ(migration.size(), 0);

  auto forward_result = migration.forward_sql();
  ASSERT_TRUE(forward_result) << "Failed to generate forward SQL: "
                              << forward_result.error().format();
  const auto& forward_sqls = *forward_result;
  EXPECT_EQ(forward_sqls.size(), 0);

  auto rollback_result = migration.rollback_sql();
  ASSERT_TRUE(rollback_result) << "Failed to generate rollback SQL: "
                               << rollback_result.error().format();
  const auto& rollback_sqls = *rollback_result;
  EXPECT_EQ(rollback_sqls.size(), 0);
}

TEST(MigrationsTest, MigrationNaming) {
  auto migration_result = migrations::generate_migration(users_v1, users_v2);
  ASSERT_TRUE(migration_result) << "Failed to generate migration: "
                                << migration_result.error().format();
  const auto& migration = *migration_result;

  EXPECT_EQ(migration.name(), "diff_users_to_users");

  auto create_result = migrations::generate_create_table_migration(users_v1);
  ASSERT_TRUE(create_result) << "Failed to generate create migration: "
                             << create_result.error().format();
  const auto& create_migration = *create_result;
  EXPECT_EQ(create_migration.name(), "create_users");

  auto drop_result = migrations::generate_drop_table_migration(users_v1);
  ASSERT_TRUE(drop_result) << "Failed to generate drop migration: " << drop_result.error().format();
  const auto& drop_migration = *drop_result;
  EXPECT_EQ(drop_migration.name(), "drop_users");
}

// Demo test that shows the library in action
TEST(MigrationsTest, DemoUsage) {
  // Generate migration from V1 to V2
  auto migration_result = migrations::generate_migration(users_v1, users_v2);

  ASSERT_TRUE(migration_result) << "Failed to generate migration: "
                                << migration_result.error().format();
  const auto& migration = *migration_result;

  std::cout << "\n=== Migration Demo ===\n";
  std::cout << "Migration: " << migration.name() << "\n";
  std::cout << "Operations: " << migration.size() << "\n\n";

  // Show forward migration SQL
  auto forward_result = migration.forward_sql();
  ASSERT_TRUE(forward_result) << "Failed to generate forward SQL: "
                              << forward_result.error().format();
  const auto& forward_sqls = *forward_result;
  std::cout << "Forward Migration SQL:\n";
  for (size_t i = 0; i < forward_sqls.size(); ++i) {
    std::cout << (i + 1) << ". " << forward_sqls[i] << "\n";
  }

  // Show rollback migration SQL
  auto rollback_result = migration.rollback_sql();
  ASSERT_TRUE(rollback_result) << "Failed to generate rollback SQL: "
                               << rollback_result.error().format();
  const auto& rollback_sqls = *rollback_result;
  std::cout << "\nRollback Migration SQL:\n";
  for (size_t i = 0; i < rollback_sqls.size(); ++i) {
    std::cout << (i + 1) << ". " << rollback_sqls[i] << "\n";
  }

  std::cout << "\n=== End Demo ===\n\n";

  SUCCEED();  // This is just a demo, always pass
}

// Test structs for comprehensive coverage analysis

// clang-format off

struct [[=relx::table("test_table")]] OriginalTable {
  [[=relx::ann::pk]] int id;
  std::string name;
  int age;  // int type
};

struct [[=relx::table("test_table")]] ModifiedTypeTable {
  [[=relx::ann::pk]] int id;
  std::string name;
  std::string age;  // Changed to string type
};

struct [[=relx::table("constraint_test")]] TableWithoutConstraints {
  [[=relx::ann::pk]] int id;
  std::string email;
  std::string username;
};

// Annotation order fixes the generated constraint names: email is constraint_test_unique_0,
// username is constraint_test_unique_1
struct [[=relx::table("constraint_test"),
        =relx::ann::composite_unique("email"),
        =relx::ann::composite_unique("username")]] TableWithConstraints {
  [[=relx::ann::pk]] int id;
  std::string email;
  std::string username;
};

struct [[=relx::table("nullable_test")]] NullableTable {
  [[=relx::ann::pk]] int id;
  std::optional<std::string> optional_field;
};

struct [[=relx::table("nullable_test")]] NonNullableTable {
  [[=relx::ann::pk]] int id;
  std::string optional_field;  // Made non-nullable
};

struct [[=relx::table("defaults_test")]] TableNoDefaults {
  [[=relx::ann::pk]] int id;
  std::string status;
};

struct [[=relx::table("defaults_test")]] TableWithDefaults {
  [[=relx::ann::pk]] int id;
  [[=relx::string_default<"active">{}]] std::string status;  // Added default
};

struct [[=relx::table("index_test")]] TableWithoutIndex {
  [[=relx::ann::pk]] int id;
  std::string name;
  std::string email;
};

struct [[=relx::table("index_test"), =relx::ann::index_on("email")]] TableWithIndex {
  [[=relx::ann::pk]] int id;
  std::string name;
  std::string email;
};

// clang-format on

inline constexpr auto original_table = relx::t<OriginalTable>;
inline constexpr auto modified_type_table = relx::t<ModifiedTypeTable>;
inline constexpr auto table_no_constraints = relx::t<TableWithoutConstraints>;
inline constexpr auto table_with_constraints = relx::t<TableWithConstraints>;
inline constexpr auto nullable_table = relx::t<NullableTable>;
inline constexpr auto non_nullable_table = relx::t<NonNullableTable>;
inline constexpr auto table_no_defaults = relx::t<TableNoDefaults>;
inline constexpr auto table_with_defaults = relx::t<TableWithDefaults>;
inline constexpr auto table_no_index = relx::t<TableWithoutIndex>;
inline constexpr auto table_with_index = relx::t<TableWithIndex>;

TEST(MigrationsTest, ComprehensiveCoverageAnalysis) {
  std::cout << "\n=== Migration Coverage Analysis ===" << std::endl;

  // Test 1: Column Type Changes (MODIFY_COLUMN)
  std::cout << "\n1. Testing Column Type Changes..." << std::endl;

  auto type_migration_result = migrations::generate_migration(original_table, modified_type_table);
  ASSERT_TRUE(type_migration_result)
      << "Failed to generate type migration: " << type_migration_result.error().format();
  const auto& type_migration = *type_migration_result;

  auto type_forward_result = type_migration.forward_sql();
  ASSERT_TRUE(type_forward_result)
      << "Failed to generate type forward SQL: " << type_forward_result.error().format();
  const auto& type_forward = *type_forward_result;

  auto type_rollback_result = type_migration.rollback_sql();
  ASSERT_TRUE(type_rollback_result)
      << "Failed to generate type rollback SQL: " << type_rollback_result.error().format();
  const auto& type_rollback = *type_rollback_result;

  std::cout << "Type change migration operations: " << type_migration.size() << std::endl;
  std::cout << "Forward SQL count: " << type_forward.size() << std::endl;
  std::cout << "Rollback SQL count: " << type_rollback.size() << std::endl;

  for (size_t i = 0; i < type_forward.size(); ++i) {
    std::cout << "Forward[" << i << "]: " << type_forward[i] << std::endl;
  }
  for (size_t i = 0; i < type_rollback.size(); ++i) {
    std::cout << "Rollback[" << i << "]: " << type_rollback[i] << std::endl;
  }

  // A type change is expressed as drop-then-add of the column
  ASSERT_EQ(type_forward.size(), 2);
  EXPECT_EQ(type_forward[0], "ALTER TABLE test_table DROP COLUMN age;");
  EXPECT_EQ(type_forward[1], "ALTER TABLE test_table ADD COLUMN age TEXT NOT NULL;");
  ASSERT_EQ(type_rollback.size(), 2);
  EXPECT_EQ(type_rollback[0], "ALTER TABLE test_table DROP COLUMN age;");
  EXPECT_EQ(type_rollback[1], "ALTER TABLE test_table ADD COLUMN age INTEGER NOT NULL;");

  // Test 2: Constraint Changes
  std::cout << "\n2. Testing Constraint Changes..." << std::endl;

  auto constraint_migration_result = migrations::generate_migration(table_no_constraints,
                                                                    table_with_constraints);
  ASSERT_TRUE(constraint_migration_result) << "Failed to generate constraint migration: "
                                           << constraint_migration_result.error().format();
  const auto& constraint_migration = *constraint_migration_result;

  auto constraint_forward_result = constraint_migration.forward_sql();
  ASSERT_TRUE(constraint_forward_result) << "Failed to generate constraint forward SQL: "
                                         << constraint_forward_result.error().format();
  const auto& constraint_forward = *constraint_forward_result;

  auto constraint_rollback_result = constraint_migration.rollback_sql();
  ASSERT_TRUE(constraint_rollback_result) << "Failed to generate constraint rollback SQL: "
                                          << constraint_rollback_result.error().format();
  const auto& constraint_rollback = *constraint_rollback_result;

  std::cout << "Constraint migration operations: " << constraint_migration.size() << std::endl;
  std::cout << "Forward SQL count: " << constraint_forward.size() << std::endl;
  std::cout << "Rollback SQL count: " << constraint_rollback.size() << std::endl;

  for (size_t i = 0; i < constraint_forward.size(); ++i) {
    std::cout << "Constraint Forward[" << i << "]: " << constraint_forward[i] << std::endl;
  }
  for (size_t i = 0; i < constraint_rollback.size(); ++i) {
    std::cout << "Constraint Rollback[" << i << "]: " << constraint_rollback[i] << std::endl;
  }

  ASSERT_EQ(constraint_forward.size(), 2);
  EXPECT_EQ(constraint_forward[0],
            "ALTER TABLE constraint_test ADD CONSTRAINT constraint_test_unique_0 UNIQUE (email);");
  EXPECT_EQ(
      constraint_forward[1],
      "ALTER TABLE constraint_test ADD CONSTRAINT constraint_test_unique_1 UNIQUE (username);");
  ASSERT_EQ(constraint_rollback.size(), 2);
  EXPECT_EQ(constraint_rollback[0],
            "ALTER TABLE constraint_test DROP CONSTRAINT constraint_test_unique_1;");
  EXPECT_EQ(constraint_rollback[1],
            "ALTER TABLE constraint_test DROP CONSTRAINT constraint_test_unique_0;");

  // Test 3: Nullable to Non-Nullable Changes
  std::cout << "\n3. Testing Nullability Changes..." << std::endl;

  auto nullable_migration_result = migrations::generate_migration(nullable_table,
                                                                  non_nullable_table);
  ASSERT_TRUE(nullable_migration_result)
      << "Failed to generate nullable migration: " << nullable_migration_result.error().format();
  const auto& nullable_migration = *nullable_migration_result;

  auto nullable_forward_result = nullable_migration.forward_sql();
  ASSERT_TRUE(nullable_forward_result)
      << "Failed to generate nullable forward SQL: " << nullable_forward_result.error().format();
  const auto& nullable_forward = *nullable_forward_result;

  auto nullable_rollback_result = nullable_migration.rollback_sql();
  ASSERT_TRUE(nullable_rollback_result)
      << "Failed to generate nullable rollback SQL: " << nullable_rollback_result.error().format();
  const auto& nullable_rollback = *nullable_rollback_result;

  std::cout << "Nullability migration operations: " << nullable_migration.size() << std::endl;
  std::cout << "Forward SQL count: " << nullable_forward.size() << std::endl;
  std::cout << "Rollback SQL count: " << nullable_rollback.size() << std::endl;

  for (size_t i = 0; i < nullable_forward.size(); ++i) {
    std::cout << "Nullable Forward[" << i << "]: " << nullable_forward[i] << std::endl;
  }
  for (size_t i = 0; i < nullable_rollback.size(); ++i) {
    std::cout << "Nullable Rollback[" << i << "]: " << nullable_rollback[i] << std::endl;
  }

  ASSERT_EQ(nullable_forward.size(), 2);
  EXPECT_EQ(nullable_forward[0], "ALTER TABLE nullable_test DROP COLUMN optional_field;");
  EXPECT_EQ(nullable_forward[1],
            "ALTER TABLE nullable_test ADD COLUMN optional_field TEXT NOT NULL;");
  ASSERT_EQ(nullable_rollback.size(), 2);
  EXPECT_EQ(nullable_rollback[0], "ALTER TABLE nullable_test DROP COLUMN optional_field;");
  EXPECT_EQ(nullable_rollback[1], "ALTER TABLE nullable_test ADD COLUMN optional_field TEXT;");

  // Test 4: Default Value Changes
  std::cout << "\n4. Testing Default Value Changes..." << std::endl;

  auto defaults_migration_result = migrations::generate_migration(table_no_defaults,
                                                                  table_with_defaults);
  ASSERT_TRUE(defaults_migration_result)
      << "Failed to generate defaults migration: " << defaults_migration_result.error().format();
  const auto& defaults_migration = *defaults_migration_result;

  auto defaults_forward_result = defaults_migration.forward_sql();
  ASSERT_TRUE(defaults_forward_result)
      << "Failed to generate defaults forward SQL: " << defaults_forward_result.error().format();
  const auto& defaults_forward = *defaults_forward_result;

  auto defaults_rollback_result = defaults_migration.rollback_sql();
  ASSERT_TRUE(defaults_rollback_result)
      << "Failed to generate defaults rollback SQL: " << defaults_rollback_result.error().format();
  const auto& defaults_rollback = *defaults_rollback_result;

  std::cout << "Defaults migration operations: " << defaults_migration.size() << std::endl;
  std::cout << "Forward SQL count: " << defaults_forward.size() << std::endl;
  std::cout << "Rollback SQL count: " << defaults_rollback.size() << std::endl;

  for (size_t i = 0; i < defaults_forward.size(); ++i) {
    std::cout << "Defaults Forward[" << i << "]: " << defaults_forward[i] << std::endl;
  }
  for (size_t i = 0; i < defaults_rollback.size(); ++i) {
    std::cout << "Defaults Rollback[" << i << "]: " << defaults_rollback[i] << std::endl;
  }

  ASSERT_EQ(defaults_forward.size(), 2);
  EXPECT_EQ(defaults_forward[0], "ALTER TABLE defaults_test DROP COLUMN status;");
  EXPECT_EQ(defaults_forward[1],
            "ALTER TABLE defaults_test ADD COLUMN status TEXT NOT NULL DEFAULT 'active';");
  ASSERT_EQ(defaults_rollback.size(), 2);
  EXPECT_EQ(defaults_rollback[0], "ALTER TABLE defaults_test DROP COLUMN status;");
  EXPECT_EQ(defaults_rollback[1], "ALTER TABLE defaults_test ADD COLUMN status TEXT NOT NULL;");

  std::cout << "\n=== Not covered by the diff engine ===" << std::endl;
  std::cout << "Column renaming: only via MigrationOptions::column_mappings" << std::endl;
  std::cout << "Table renaming: not supported" << std::endl;
}

TEST(MigrationsTest, TestDropConstraintOperations) {
  std::cout << "\n=== Testing DROP_CONSTRAINT Operations ===" << std::endl;

  // Test dropping constraints (going from constraints to no constraints)
  auto drop_constraint_migration_result = migrations::generate_migration(table_with_constraints,
                                                                         table_no_constraints);
  ASSERT_TRUE(drop_constraint_migration_result)
      << "Failed to generate drop constraint migration: "
      << drop_constraint_migration_result.error().format();
  const auto& drop_constraint_migration = *drop_constraint_migration_result;

  auto drop_forward_result = drop_constraint_migration.forward_sql();
  ASSERT_TRUE(drop_forward_result)
      << "Failed to generate drop forward SQL: " << drop_forward_result.error().format();
  const auto& drop_forward = *drop_forward_result;

  auto drop_rollback_result = drop_constraint_migration.rollback_sql();
  ASSERT_TRUE(drop_rollback_result)
      << "Failed to generate drop rollback SQL: " << drop_rollback_result.error().format();
  const auto& drop_rollback = *drop_rollback_result;

  std::cout << "Drop constraint operations: " << drop_constraint_migration.size() << std::endl;
  std::cout << "Forward SQL count: " << drop_forward.size() << std::endl;
  std::cout << "Rollback SQL count: " << drop_rollback.size() << std::endl;

  for (size_t i = 0; i < drop_forward.size(); ++i) {
    std::cout << "Drop Forward[" << i << "]: " << drop_forward[i] << std::endl;
  }
  for (size_t i = 0; i < drop_rollback.size(); ++i) {
    std::cout << "Drop Rollback[" << i << "]: " << drop_rollback[i] << std::endl;
  }

  // Test exact SQL for dropping constraints (deterministic: alphabetical by constraint name)
  ASSERT_EQ(drop_forward.size(), 2);
  EXPECT_EQ(drop_forward[0],
            "ALTER TABLE constraint_test DROP CONSTRAINT constraint_test_unique_0;");
  EXPECT_EQ(drop_forward[1],
            "ALTER TABLE constraint_test DROP CONSTRAINT constraint_test_unique_1;");

  // Test exact rollback SQL for adding constraints back
  ASSERT_EQ(drop_rollback.size(), 2);
  EXPECT_EQ(
      drop_rollback[0],
      "ALTER TABLE constraint_test ADD CONSTRAINT constraint_test_unique_1 UNIQUE (username);");
  EXPECT_EQ(drop_rollback[1],
            "ALTER TABLE constraint_test ADD CONSTRAINT constraint_test_unique_0 UNIQUE (email);");
}

// clang-format off

struct [[=relx::table("inline_unique_test")]] InlineUniqueV1 {
  int id;
  std::string email;
};

struct [[=relx::table("inline_unique_test")]] InlineUniqueV2 {
  int id;
  [[=relx::ann::unique]] std::string email;
};

// clang-format on

// Adding an inline [[=relx::ann::unique]] must diff as a constraint operation.
// (It previously diffed as a changed column definition -> DROP COLUMN + ADD
// COLUMN, silently destroying the column's data.)
TEST(MigrationsTest, InlineUniqueDiffsAsConstraintNotColumnRebuild) {
  auto migration_result = migrations::generate_migration(relx::t<InlineUniqueV1>,
                                                         relx::t<InlineUniqueV2>);
  ASSERT_TRUE(migration_result) << migration_result.error().format();

  auto forward = migration_result->forward_sql();
  ASSERT_TRUE(forward) << forward.error().format();
  ASSERT_EQ(forward->size(), 1);
  EXPECT_EQ(
      (*forward)[0],
      "ALTER TABLE inline_unique_test ADD CONSTRAINT inline_unique_test_unique_0 UNIQUE (email);");

  auto rollback = migration_result->rollback_sql();
  ASSERT_TRUE(rollback) << rollback.error().format();
  ASSERT_EQ(rollback->size(), 1);
  EXPECT_EQ((*rollback)[0],
            "ALTER TABLE inline_unique_test DROP CONSTRAINT inline_unique_test_unique_0;");
}

// clang-format off

struct [[=relx::table("hoist_ref")]] HoistRefTarget {
  [[=relx::ann::pk]] int id;
};

struct [[=relx::table("hoist_test")]] HoistV1 {
  [[=relx::ann::pk]] int id;
  int user_id;
  int quantity;
};

struct [[=relx::table("hoist_test")]] HoistV2 {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^HoistRefTarget::id>, =relx::on_delete<"CASCADE">{}]] int user_id;
  [[=relx::schema::check<"quantity > 0">{}]] int quantity;
};

// clang-format on

// Inline fk and check modifiers hoist into table-level constraints like unique does:
// adding them diffs as ADD CONSTRAINT (with the differ's deterministic name), not as a
// data-destroying column rebuild.
TEST(MigrationsTest, InlineFkAndCheckDiffAsConstraintsNotColumnRebuild) {
  auto migration_result = migrations::generate_migration(relx::t<HoistV1>, relx::t<HoistV2>);
  ASSERT_TRUE(migration_result) << migration_result.error().format();

  auto forward = migration_result->forward_sql();
  ASSERT_TRUE(forward) << forward.error().format();
  ASSERT_EQ(forward->size(), 2);
  EXPECT_EQ((*forward)[0],
            "ALTER TABLE hoist_test ADD CONSTRAINT hoist_test_check_1 CHECK (quantity > 0);");
  EXPECT_EQ((*forward)[1], "ALTER TABLE hoist_test ADD CONSTRAINT hoist_test_fk_0 FOREIGN KEY "
                           "(user_id) REFERENCES hoist_ref(id) ON DELETE CASCADE;");

  auto rollback = migration_result->rollback_sql();
  ASSERT_TRUE(rollback) << rollback.error().format();
  ASSERT_EQ(rollback->size(), 2);
  EXPECT_EQ((*rollback)[0], "ALTER TABLE hoist_test DROP CONSTRAINT hoist_test_fk_0;");
  EXPECT_EQ((*rollback)[1], "ALTER TABLE hoist_test DROP CONSTRAINT hoist_test_check_1;");
}

// clang-format off

struct [[=relx::table("named_check_test")]] NamedCheckV1 {
  int amount;
};

struct [[=relx::table("named_check_test"),
        =relx::ann::check("amount > 0").named("amount_positive")]] NamedCheckV2 {
  int amount;
};

// clang-format on

// A .named() table-level check must keep its explicit name in the diff metadata:
// the DROP CONSTRAINT in the rollback has to target the name that was actually
// created, not a generated positional one (named_check_test_check_0)
TEST(MigrationsTest, NamedCheckConstraintKeepsItsNameInDiffs) {
  auto migration_result = migrations::generate_migration(relx::t<NamedCheckV1>,
                                                         relx::t<NamedCheckV2>);
  ASSERT_TRUE(migration_result) << migration_result.error().format();

  auto forward = migration_result->forward_sql();
  ASSERT_TRUE(forward) << forward.error().format();
  ASSERT_EQ(forward->size(), 1);
  EXPECT_EQ((*forward)[0],
            "ALTER TABLE named_check_test ADD CONSTRAINT amount_positive CHECK (amount > 0);");

  auto rollback = migration_result->rollback_sql();
  ASSERT_TRUE(rollback) << rollback.error().format();
  ASSERT_EQ(rollback->size(), 1);
  EXPECT_EQ((*rollback)[0], "ALTER TABLE named_check_test DROP CONSTRAINT amount_positive;");
}

TEST(MigrationsTest, TestIndexOperations) {
  std::cout << "\n=== Testing INDEX Operations ===" << std::endl;

  // An index_on struct annotation is diffed as its own operation kind: CREATE INDEX
  // rather than ALTER TABLE
  auto add_index_migration_result = migrations::generate_migration(table_no_index,
                                                                   table_with_index);
  ASSERT_TRUE(add_index_migration_result)
      << "Failed to generate add index migration: " << add_index_migration_result.error().format();
  const auto& add_index_migration = *add_index_migration_result;

  auto add_index_forward_result = add_index_migration.forward_sql();
  ASSERT_TRUE(add_index_forward_result)
      << "Failed to generate add index forward SQL: " << add_index_forward_result.error().format();
  const auto& add_index_forward = *add_index_forward_result;

  auto add_index_rollback_result = add_index_migration.rollback_sql();
  ASSERT_TRUE(add_index_rollback_result) << "Failed to generate add index rollback SQL: "
                                         << add_index_rollback_result.error().format();
  const auto& add_index_rollback = *add_index_rollback_result;

  std::cout << "Add index migration operations: " << add_index_migration.size() << std::endl;
  for (size_t i = 0; i < add_index_forward.size(); ++i) {
    std::cout << "Add Index Forward[" << i << "]: " << add_index_forward[i] << std::endl;
  }

  // Test dropping the index
  auto drop_index_migration_result = migrations::generate_migration(table_with_index,
                                                                    table_no_index);
  ASSERT_TRUE(drop_index_migration_result) << "Failed to generate drop index migration: "
                                           << drop_index_migration_result.error().format();
  const auto& drop_index_migration = *drop_index_migration_result;

  auto drop_index_forward_result = drop_index_migration.forward_sql();
  ASSERT_TRUE(drop_index_forward_result) << "Failed to generate drop index forward SQL: "
                                         << drop_index_forward_result.error().format();
  const auto& drop_index_forward = *drop_index_forward_result;

  auto drop_index_rollback_result = drop_index_migration.rollback_sql();
  ASSERT_TRUE(drop_index_rollback_result) << "Failed to generate drop index rollback SQL: "
                                          << drop_index_rollback_result.error().format();
  const auto& drop_index_rollback = *drop_index_rollback_result;

  std::cout << "Drop index migration operations: " << drop_index_migration.size() << std::endl;
  for (size_t i = 0; i < drop_index_forward.size(); ++i) {
    std::cout << "Drop Index Forward[" << i << "]: " << drop_index_forward[i] << std::endl;
  }

  ASSERT_EQ(add_index_forward.size(), 1);
  EXPECT_EQ(add_index_forward[0], "CREATE INDEX index_test_email_idx ON index_test (email);");

  ASSERT_EQ(add_index_rollback.size(), 1);
  EXPECT_EQ(add_index_rollback[0], "DROP INDEX IF EXISTS index_test_email_idx;");

  ASSERT_EQ(drop_index_forward.size(), 1);
  EXPECT_EQ(drop_index_forward[0], "DROP INDEX IF EXISTS index_test_email_idx;");

  ASSERT_EQ(drop_index_rollback.size(), 1);
  EXPECT_EQ(drop_index_rollback[0], "CREATE INDEX index_test_email_idx ON index_test (email);");
}

// Define table structures for column renaming tests

// clang-format off

struct [[=relx::table("employees")]] OldEmployeeTable {
  [[=relx::ann::pk]] int id;
  std::string first_name;
  std::string last_name;
  std::string email_addr;
  std::optional<std::string> phone;
};

struct [[=relx::table("employees")]] NewEmployeeTable {
  [[=relx::ann::pk]] int id;
  std::string given_name;                   // renamed from first_name
  std::string family_name;                  // renamed from last_name
  std::string email;                        // renamed from email_addr
  std::optional<std::string> phone_number;  // renamed from phone
};

// clang-format on

inline constexpr auto old_employees = relx::t<OldEmployeeTable>;
inline constexpr auto new_employees = relx::t<NewEmployeeTable>;

TEST(MigrationsTest, TestColumnRenaming) {
  std::cout << "\n=== Testing Column Renaming ===" << std::endl;

  // Test 1: Without mappings - should see drop/add operations (data loss)
  std::cout << "\n1. Migration WITHOUT column mappings (data loss):" << std::endl;
  auto migration_without_mappings_result = migrations::generate_migration(old_employees,
                                                                          new_employees);
  ASSERT_TRUE(migration_without_mappings_result)
      << "Failed to generate migration without mappings: "
      << migration_without_mappings_result.error().format();
  const auto& migration_without_mappings = *migration_without_mappings_result;

  auto forward_no_mappings_result = migration_without_mappings.forward_sql();
  ASSERT_TRUE(forward_no_mappings_result) << "Failed to generate forward SQL without mappings: "
                                          << forward_no_mappings_result.error().format();
  const auto& forward_no_mappings = *forward_no_mappings_result;

  auto rollback_no_mappings_result = migration_without_mappings.rollback_sql();
  ASSERT_TRUE(rollback_no_mappings_result) << "Failed to generate rollback SQL without mappings: "
                                           << rollback_no_mappings_result.error().format();
  const auto& rollback_no_mappings = *rollback_no_mappings_result;
  EXPECT_EQ(rollback_no_mappings.size(), 8);

  std::cout << "Operations without mappings: " << migration_without_mappings.size() << std::endl;
  for (size_t i = 0; i < forward_no_mappings.size(); ++i) {
    std::cout << "Forward[" << i << "]: " << forward_no_mappings[i] << std::endl;
  }

  // Should generate 8 operations: 4 drops + 4 adds (excluding id which is unchanged)
  EXPECT_EQ(migration_without_mappings.size(), 8);

  // Test 2: With column mappings - should see rename operations (data preserved)
  std::cout << "\n2. Migration WITH column mappings (data preserved):" << std::endl;
  migrations::MigrationOptions options;
  options.column_mappings = {{"first_name", "given_name"},
                             {"last_name", "family_name"},
                             {"email_addr", "email"},
                             {"phone", "phone_number"}};

  auto migration_with_mappings_result = migrations::generate_migration(old_employees, new_employees,
                                                                       options);
  ASSERT_TRUE(migration_with_mappings_result) << "Failed to generate migration with mappings: "
                                              << migration_with_mappings_result.error().format();
  const auto& migration_with_mappings = *migration_with_mappings_result;

  auto forward_with_mappings_result = migration_with_mappings.forward_sql();
  ASSERT_TRUE(forward_with_mappings_result) << "Failed to generate forward SQL with mappings: "
                                            << forward_with_mappings_result.error().format();
  const auto& forward_with_mappings = *forward_with_mappings_result;

  auto rollback_with_mappings_result = migration_with_mappings.rollback_sql();
  ASSERT_TRUE(rollback_with_mappings_result) << "Failed to generate rollback SQL with mappings: "
                                             << rollback_with_mappings_result.error().format();
  const auto& rollback_with_mappings = *rollback_with_mappings_result;

  std::cout << "Operations with mappings: " << migration_with_mappings.size() << std::endl;
  for (size_t i = 0; i < forward_with_mappings.size(); ++i) {
    std::cout << "Forward[" << i << "]: " << forward_with_mappings[i] << std::endl;
  }
  for (size_t i = 0; i < rollback_with_mappings.size(); ++i) {
    std::cout << "Rollback[" << i << "]: " << rollback_with_mappings[i] << std::endl;
  }

  // Should generate 4 rename operations
  ASSERT_EQ(migration_with_mappings.size(), 4);

  // Test exact SQL for renames (deterministic: alphabetical by old column name)
  EXPECT_EQ(forward_with_mappings[0], "ALTER TABLE employees RENAME COLUMN email_addr TO email;");
  EXPECT_EQ(forward_with_mappings[1],
            "ALTER TABLE employees RENAME COLUMN first_name TO given_name;");
  EXPECT_EQ(forward_with_mappings[2],
            "ALTER TABLE employees RENAME COLUMN last_name TO family_name;");
  EXPECT_EQ(forward_with_mappings[3], "ALTER TABLE employees RENAME COLUMN phone TO phone_number;");

  // Test exact rollback SQL (reverse order)
  EXPECT_EQ(rollback_with_mappings[0],
            "ALTER TABLE employees RENAME COLUMN phone_number TO phone;");
  EXPECT_EQ(rollback_with_mappings[1],
            "ALTER TABLE employees RENAME COLUMN family_name TO last_name;");
  EXPECT_EQ(rollback_with_mappings[2],
            "ALTER TABLE employees RENAME COLUMN given_name TO first_name;");
  EXPECT_EQ(rollback_with_mappings[3], "ALTER TABLE employees RENAME COLUMN email TO email_addr;");
}

// Define table structures for column rename + type change tests

// clang-format off

struct [[=relx::table("products")]] OldProductTable {
  [[=relx::ann::pk]] int id;
  int price_cents;  // int price in cents
};

struct [[=relx::table("products")]] NewProductTable {
  [[=relx::ann::pk]] int id;
  std::string price_dollars;  // string price in dollars
};

// clang-format on

inline constexpr auto old_products = relx::t<OldProductTable>;
inline constexpr auto new_products = relx::t<NewProductTable>;

TEST(MigrationsTest, TestColumnRenameWithTypeChange) {
  std::cout << "\n=== Testing Column Rename + Type Change ===" << std::endl;

  // Test with mapping for rename + type change
  migrations::MigrationOptions options;
  options.column_mappings = {{"price_cents", "price_dollars"}};

  auto migration_result = migrations::generate_migration(old_products, new_products, options);
  ASSERT_TRUE(migration_result) << "Failed to generate migration: "
                                << migration_result.error().format();
  const auto& migration = *migration_result;

  auto forward_sql_result = migration.forward_sql();
  ASSERT_TRUE(forward_sql_result) << "Failed to generate forward SQL: "
                                  << forward_sql_result.error().format();
  const auto& forward_sql = *forward_sql_result;

  auto rollback_sql_result = migration.rollback_sql();
  ASSERT_TRUE(rollback_sql_result)
      << "Failed to generate rollback SQL: " << rollback_sql_result.error().format();
  const auto& rollback_sql = *rollback_sql_result;

  std::cout << "Rename + type change operations: " << migration.size() << std::endl;
  for (size_t i = 0; i < forward_sql.size(); ++i) {
    std::cout << "Forward[" << i << "]: " << forward_sql[i] << std::endl;
  }
  for (size_t i = 0; i < rollback_sql.size(); ++i) {
    std::cout << "Rollback[" << i << "]: " << rollback_sql[i] << std::endl;
  }

  // Should generate: 1 add + 1 drop (for rename + type change)
  // Note: This strategy preserves data by requiring manual UPDATE between ADD and DROP
  ASSERT_EQ(migration.size(), 2);

  // Verify the operations
  EXPECT_EQ(forward_sql[0], "ALTER TABLE products ADD COLUMN price_dollars TEXT NOT NULL;");
  EXPECT_EQ(forward_sql[1], "ALTER TABLE products DROP COLUMN price_cents;");
}

TEST(MigrationsTest, TestBidirectionalTransformations) {
  std::cout << "\n=== Testing Bidirectional Column Transformations ===" << std::endl;

  // Test with bidirectional transformations
  migrations::MigrationOptions options;
  options.column_mappings = {{"price_cents", "price_dollars"}};
  options.column_transformations = {
      {"price_cents",
       {
           "CAST(price_cents / 100.0 AS TEXT) || ' USD'",  // forward: cents -> dollars string
           "CAST(REPLACE(price_dollars, ' USD', '') AS DECIMAL) * 100"  // backward: dollars string
                                                                        // -> cents
       }}};

  auto migration_result = migrations::generate_migration(old_products, new_products, options);
  ASSERT_TRUE(migration_result) << "Failed to generate migration: "
                                << migration_result.error().format();
  const auto& migration = *migration_result;

  auto forward_sql_result = migration.forward_sql();
  ASSERT_TRUE(forward_sql_result) << "Failed to generate forward SQL: "
                                  << forward_sql_result.error().format();
  const auto& forward_sql = *forward_sql_result;

  auto rollback_sql_result = migration.rollback_sql();
  ASSERT_TRUE(rollback_sql_result)
      << "Failed to generate rollback SQL: " << rollback_sql_result.error().format();
  const auto& rollback_sql = *rollback_sql_result;

  std::cout << "Bidirectional transformation operations: " << migration.size() << std::endl;
  for (size_t i = 0; i < forward_sql.size(); ++i) {
    std::cout << "Forward[" << i << "]: " << forward_sql[i] << std::endl;
  }
  for (size_t i = 0; i < rollback_sql.size(); ++i) {
    std::cout << "Rollback[" << i << "]: " << rollback_sql[i] << std::endl;
  }

  // Should generate: 1 ADD + 1 UPDATE + 1 DROP (for rename + type change with transformation)
  ASSERT_EQ(migration.size(), 3);

  // Verify forward migration SQL
  EXPECT_EQ(forward_sql[0], "ALTER TABLE products ADD COLUMN price_dollars TEXT NOT NULL;");
  EXPECT_EQ(forward_sql[1],
            "UPDATE products SET price_dollars = CAST(price_cents / 100.0 AS TEXT) || ' USD';");
  EXPECT_EQ(forward_sql[2], "ALTER TABLE products DROP COLUMN price_cents;");

  // Verify rollback migration SQL (reverse order)
  EXPECT_EQ(rollback_sql[0], "ALTER TABLE products ADD COLUMN price_cents INTEGER NOT NULL;");
  EXPECT_EQ(rollback_sql[1], "UPDATE products SET price_cents = CAST(REPLACE(price_dollars, ' "
                             "USD', '') AS DECIMAL) * 100;");
  EXPECT_EQ(rollback_sql[2], "ALTER TABLE products DROP COLUMN price_dollars;");
}

}  // namespace
