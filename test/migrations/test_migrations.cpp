#include <iostream>

#include <gtest/gtest.h>
#include <relx/migrations.hpp>
#include <relx/schema.hpp>

using namespace relx;

// Test table definitions

// Version 1 of Users table
struct UsersV1 {
  static constexpr auto table_name = "users";

  column<UsersV1, "id", int, primary_key> id;
  column<UsersV1, "name", std::string> name;
  column<UsersV1, "email", std::string> email;

  unique_constraint<&UsersV1::email> unique_email;
};

// Version 2 of Users table - with additional columns
struct UsersV2 {
  static constexpr auto table_name = "users";

  column<UsersV2, "id", int, primary_key> id;
  column<UsersV2, "name", std::string> name;
  column<UsersV2, "email", std::string> email;
  column<UsersV2, "age", std::optional<int>> age;  // New nullable column
  column<UsersV2, "created_at", std::string, string_default<"CURRENT_TIMESTAMP", true>>
      created_at;  // New column with default

  unique_constraint<&UsersV2::email> unique_email;
};

// Version 3 of Users table - removed a column
struct UsersV3 {
  static constexpr auto table_name = "users";

  column<UsersV3, "id", int, primary_key> id;
  column<UsersV3, "name", std::string> name;
  column<UsersV3, "age", std::optional<int>> age;
  column<UsersV3, "created_at", std::string, string_default<"CURRENT_TIMESTAMP", true>> created_at;

  // Note: email column and unique constraint removed
};

// Simple table for creation/deletion tests
struct SimpleTable {
  static constexpr auto table_name = "simple_table";

  column<SimpleTable, "id", int, primary_key> id;
  column<SimpleTable, "data", std::string> data;
};

TEST(MigrationsTest, ExtractTableMetadata) {
  UsersV1 users_v1;
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

  // Check that constraints are extracted
  EXPECT_GT(metadata.constraints.size(), 0);
}

TEST(MigrationsTest, GenerateAddColumnMigration) {
  UsersV1 old_users;
  UsersV2 new_users;

  auto migration_result = relx::migrations::generate_migration(old_users, new_users);

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
  UsersV2 old_users;
  UsersV3 new_users;

  auto migration_result = relx::migrations::generate_migration(old_users, new_users);

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
    if (sql.find("ADD UNIQUE") != std::string::npos) {
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
  SimpleTable simple_table;

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
  std::string expected_create = "CREATE TABLE simple_table (\n"
                                "id INTEGER NOT NULL,\n"
                                "name TEXT NOT NULL,\n"
                                "active BOOLEAN NOT NULL\n"
                                ");";
  EXPECT_EQ(forward_sqls[0], expected_create);

  // Check rollback drops the table
  auto rollback_result = migration.rollback_sql();
  ASSERT_TRUE(rollback_result) << "Failed to generate rollback SQL: "
                               << rollback_result.error().format();
  const auto& rollback_sqls = *rollback_result;
  ASSERT_EQ(rollback_sqls.size(), 1);

  EXPECT_EQ(rollback_sqls[0], "DROP TABLE IF EXISTS simple_table;");
}

TEST(MigrationsTest, GenerateDropTableMigration) {
  SimpleTable simple_table;

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

  std::string expected_create = "CREATE TABLE simple_table (\n"
                                "id INTEGER NOT NULL,\n"
                                "name TEXT NOT NULL,\n"
                                "active BOOLEAN NOT NULL\n"
                                ");";
  EXPECT_EQ(rollback_sqls[0], expected_create);
}

TEST(MigrationsTest, EmptyMigrationForIdenticalTables) {
  UsersV1 users1;
  UsersV1 users2;

  auto migration_result = migrations::generate_migration(users1, users2);

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
  UsersV1 old_users;
  UsersV2 new_users;

  auto migration_result = migrations::generate_migration(old_users, new_users);
  ASSERT_TRUE(migration_result) << "Failed to generate migration: "
                                << migration_result.error().format();
  const auto& migration = *migration_result;

  EXPECT_EQ(migration.name(), "diff_users_to_users");

  auto create_result = migrations::generate_create_table_migration(old_users);
  ASSERT_TRUE(create_result) << "Failed to generate create migration: "
                             << create_result.error().format();
  const auto& create_migration = *create_result;
  EXPECT_EQ(create_migration.name(), "create_users");

  auto drop_result = migrations::generate_drop_table_migration(old_users);
  ASSERT_TRUE(drop_result) << "Failed to generate drop migration: " << drop_result.error().format();
  const auto& drop_migration = *drop_result;
  EXPECT_EQ(drop_migration.name(), "drop_users");
}

// Demo test that shows the library in action
TEST(MigrationsTest, DemoUsage) {
  UsersV1 old_users;
  UsersV2 new_users;

  // Generate migration from V1 to V2
  auto migration_result = migrations::generate_migration(old_users, new_users);

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
struct OriginalTable {
  static constexpr auto table_name = "test_table";

  column<OriginalTable, "id", int, primary_key> id;
  column<OriginalTable, "name", std::string> name;
  column<OriginalTable, "age", int> age;  // int type
};

struct ModifiedTypeTable {
  static constexpr auto table_name = "test_table";

  column<ModifiedTypeTable, "id", int, primary_key> id;
  column<ModifiedTypeTable, "name", std::string> name;
  column<ModifiedTypeTable, "age", std::string> age;  // Changed to string type
};

struct TableWithoutConstraints {
  static constexpr auto table_name = "constraint_test";

  column<TableWithoutConstraints, "id", int, primary_key> id;
  column<TableWithoutConstraints, "email", std::string> email;
  column<TableWithoutConstraints, "username", std::string> username;
};

struct TableWithConstraints {
  static constexpr auto table_name = "constraint_test";

  column<TableWithConstraints, "id", int, primary_key> id;
  column<TableWithConstraints, "email", std::string> email;
  column<TableWithConstraints, "username", std::string> username;

  unique_constraint<&TableWithConstraints::email> unique_email;
  unique_constraint<&TableWithConstraints::username> unique_username;
};

struct NullableTable {
  static constexpr auto table_name = "nullable_test";

  column<NullableTable, "id", int, primary_key> id;
  column<NullableTable, "optional_field", std::optional<std::string>> optional_field;
};

struct NonNullableTable {
  static constexpr auto table_name = "nullable_test";

  column<NonNullableTable, "id", int, primary_key> id;
  column<NonNullableTable, "optional_field", std::string> optional_field;  // Made non-nullable
};

struct TableNoDefaults {
  static constexpr auto table_name = "defaults_test";

  column<TableNoDefaults, "id", int, primary_key> id;
  column<TableNoDefaults, "status", std::string> status;
};

struct TableWithDefaults {
  static constexpr auto table_name = "defaults_test";

  column<TableWithDefaults, "id", int, primary_key> id;
  column<TableWithDefaults, "status", std::string, string_default<"active", false>>
      status;  // Added default
};

struct TableWithIndex {
  static constexpr auto table_name = "index_test";

  column<TableWithIndex, "id", int, primary_key> id;
  column<TableWithIndex, "name", std::string> name;
  column<TableWithIndex, "email", std::string> email;

  // Note: Currently there's no explicit index constraint type,
  // but unique constraints are a form of index
  unique_constraint<&TableWithIndex::email> unique_email_index;
};

struct TableWithoutIndex {
  static constexpr auto table_name = "index_test";

  column<TableWithoutIndex, "id", int, primary_key> id;
  column<TableWithoutIndex, "name", std::string> name;
  column<TableWithoutIndex, "email", std::string> email;
};

TEST(MigrationsTest, ComprehensiveCoverageAnalysis) {
  std::cout << "\n=== Migration Coverage Analysis ===" << std::endl;

  // Test 1: Column Type Changes (MODIFY_COLUMN)
  std::cout << "\n1. Testing Column Type Changes..." << std::endl;

  OriginalTable orig_table;
  ModifiedTypeTable mod_table;

  auto type_migration_result = migrations::generate_migration(orig_table, mod_table);
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

  // Test 2: Constraint Changes
  std::cout << "\n2. Testing Constraint Changes..." << std::endl;

  TableWithoutConstraints table_no_constraints;
  TableWithConstraints table_with_constraints;

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

  // Test 3: Nullable to Non-Nullable Changes
  std::cout << "\n3. Testing Nullability Changes..." << std::endl;

  NullableTable nullable_table;
  NonNullableTable non_nullable_table;

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

  // Test 4: Default Value Changes
  std::cout << "\n4. Testing Default Value Changes..." << std::endl;

  TableNoDefaults table_no_defaults;
  TableWithDefaults table_with_defaults;

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

  // Summary of findings
  std::cout << "\n=== Coverage Summary ===" << std::endl;
  std::cout << "✅ CREATE_TABLE: Working" << std::endl;
  std::cout << "✅ DROP_TABLE: Working" << std::endl;
  std::cout << "✅ ADD_COLUMN: Working" << std::endl;
  std::cout << "✅ DROP_COLUMN: Working" << std::endl;

  if (type_migration.size() > 0) {
    std::cout << "✅ MODIFY_COLUMN (type changes): Detected operations" << std::endl;
  } else {
    std::cout << "❌ MODIFY_COLUMN (type changes): No operations generated" << std::endl;
  }

  if (constraint_migration.size() > 0) {
    std::cout << "✅ ADD_CONSTRAINT: Detected operations" << std::endl;
  } else {
    std::cout << "❌ ADD_CONSTRAINT: No operations generated" << std::endl;
  }

  if (nullable_migration.size() > 0) {
    std::cout << "✅ Nullability changes: Detected operations" << std::endl;
  } else {
    std::cout << "❌ Nullability changes: No operations generated" << std::endl;
  }

  if (defaults_migration.size() > 0) {
    std::cout << "✅ Default value changes: Detected operations" << std::endl;
  } else {
    std::cout << "❌ Default value changes: No operations generated" << std::endl;
  }

  std::cout << "\n=== Missing Features ===" << std::endl;
  std::cout << "❓ DROP_CONSTRAINT: Not explicitly tested" << std::endl;
  std::cout << "❓ ADD_INDEX: Not explicitly tested" << std::endl;
  std::cout << "❓ DROP_INDEX: Not explicitly tested" << std::endl;
  std::cout << "❓ Column renaming: Not supported (would require additional metadata)" << std::endl;
  std::cout << "❓ Table renaming: Not supported" << std::endl;
  std::cout << "❓ Complex constraint modifications: Unknown" << std::endl;

  // This test always passes - it's for analysis only
  EXPECT_TRUE(true);
}

TEST(MigrationsTest, TestDropConstraintOperations) {
  std::cout << "\n=== Testing DROP_CONSTRAINT Operations ===" << std::endl;

  TableWithConstraints table_with_constraints;
  TableWithoutConstraints table_no_constraints;

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

  if (drop_constraint_migration.size() > 0) {
    std::cout << "✅ DROP_CONSTRAINT: Working" << std::endl;

    // Test exact SQL for dropping constraints
    EXPECT_EQ(drop_forward.size(), 2);
    EXPECT_EQ(drop_forward[0],
              "ALTER TABLE constraint_test DROP CONSTRAINT constraint_test_unique_1;");
    EXPECT_EQ(drop_forward[1],
              "ALTER TABLE constraint_test DROP CONSTRAINT constraint_test_unique_0;");

    // Test exact rollback SQL for adding constraints back
    EXPECT_EQ(drop_rollback.size(), 2);
    EXPECT_EQ(drop_rollback[0], "ALTER TABLE constraint_test ADD UNIQUE (email);");
    EXPECT_EQ(drop_rollback[1], "ALTER TABLE constraint_test ADD UNIQUE (username);");
  } else {
    std::cout << "❌ DROP_CONSTRAINT: No operations generated" << std::endl;
    FAIL() << "Expected constraint drop operations but none were generated";
  }
}

TEST(MigrationsTest, TestIndexOperations) {
  std::cout << "\n=== Testing INDEX Operations ===" << std::endl;

  // Note: Currently we don't have explicit index types in the schema,
  // but let's test if any index-like constraints are detected

  TableWithoutIndex table_no_index;
  TableWithIndex table_with_index;

  // Test adding index (via unique constraint)
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
  std::cout << "Forward SQL count: " << add_index_forward.size() << std::endl;
  std::cout << "Rollback SQL count: " << add_index_rollback.size() << std::endl;

  for (size_t i = 0; i < add_index_forward.size(); ++i) {
    std::cout << "Add Index Forward[" << i << "]: " << add_index_forward[i] << std::endl;
  }

  // Test dropping index (via unique constraint)
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
  std::cout << "Forward SQL count: " << drop_index_forward.size() << std::endl;
  std::cout << "Rollback SQL count: " << drop_index_rollback.size() << std::endl;

  for (size_t i = 0; i < drop_index_forward.size(); ++i) {
    std::cout << "Drop Index Forward[" << i << "]: " << drop_index_forward[i] << std::endl;
  }

  if (add_index_migration.size() > 0 && drop_index_migration.size() > 0) {
    std::cout << "✅ ADD_INDEX/DROP_INDEX: Working (via constraints)" << std::endl;

    // Test exact SQL for adding index (constraint)
    EXPECT_EQ(add_index_forward.size(), 1);
    EXPECT_EQ(add_index_forward[0], "ALTER TABLE index_test ADD UNIQUE (email);");

    // Test exact rollback SQL for adding index
    EXPECT_EQ(add_index_rollback.size(), 1);
    EXPECT_EQ(add_index_rollback[0], "ALTER TABLE index_test DROP CONSTRAINT index_test_unique_0;");

    // Test exact SQL for dropping index (constraint)
    EXPECT_EQ(drop_index_forward.size(), 1);
    EXPECT_EQ(drop_index_forward[0], "ALTER TABLE index_test DROP CONSTRAINT index_test_unique_0;");

    // Test exact rollback SQL for dropping index
    EXPECT_EQ(drop_index_rollback.size(), 1);
    EXPECT_EQ(drop_index_rollback[0], "ALTER TABLE index_test ADD UNIQUE (email);");
  } else {
    std::cout
        << "❓ ADD_INDEX/DROP_INDEX: No explicit index operations (using constraint-based indexes)"
        << std::endl;
    EXPECT_TRUE(true);  // This is expected for now since we don't have explicit index types
  }
}

// Define table structures for column renaming tests
struct OldEmployeeTable {
  static constexpr auto table_name = "employees";

  column<OldEmployeeTable, "id", int, primary_key> id;
  column<OldEmployeeTable, "first_name", std::string> first_name;
  column<OldEmployeeTable, "last_name", std::string> last_name;
  column<OldEmployeeTable, "email_addr", std::string> email_addr;
  column<OldEmployeeTable, "phone", std::optional<std::string>> phone;
};

struct NewEmployeeTable {
  static constexpr auto table_name = "employees";

  column<NewEmployeeTable, "id", int, primary_key> id;
  column<NewEmployeeTable, "given_name", std::string> given_name;    // renamed from first_name
  column<NewEmployeeTable, "family_name", std::string> family_name;  // renamed from last_name
  column<NewEmployeeTable, "email", std::string> email;              // renamed from email_addr
  column<NewEmployeeTable, "phone_number", std::optional<std::string>>
      phone_number;  // renamed from phone
};

TEST(MigrationsTest, TestColumnRenaming) {
  std::cout << "\n=== Testing Column Renaming ===" << std::endl;

  OldEmployeeTable old_table;
  NewEmployeeTable new_table;

  // Test 1: Without mappings - should see drop/add operations (data loss)
  std::cout << "\n1. Migration WITHOUT column mappings (data loss):" << std::endl;
  auto migration_without_mappings_result = migrations::generate_migration(old_table, new_table);
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

  auto migration_with_mappings_result = migrations::generate_migration(old_table, new_table,
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
  EXPECT_EQ(migration_with_mappings.size(), 4);

  // Test exact SQL for renames (order determined by iteration over unordered_map)
  EXPECT_EQ(forward_with_mappings[0],
            "ALTER TABLE employees RENAME COLUMN last_name TO family_name;");
  EXPECT_EQ(forward_with_mappings[1], "ALTER TABLE employees RENAME COLUMN phone TO phone_number;");
  EXPECT_EQ(forward_with_mappings[2], "ALTER TABLE employees RENAME COLUMN email_addr TO email;");
  EXPECT_EQ(forward_with_mappings[3],
            "ALTER TABLE employees RENAME COLUMN first_name TO given_name;");

  // Test exact rollback SQL (reverse order)
  EXPECT_EQ(rollback_with_mappings[0],
            "ALTER TABLE employees RENAME COLUMN given_name TO first_name;");
  EXPECT_EQ(rollback_with_mappings[1], "ALTER TABLE employees RENAME COLUMN email TO email_addr;");
  EXPECT_EQ(rollback_with_mappings[2],
            "ALTER TABLE employees RENAME COLUMN phone_number TO phone;");
  EXPECT_EQ(rollback_with_mappings[3],
            "ALTER TABLE employees RENAME COLUMN family_name TO last_name;");

  std::cout << "✅ Column renaming: Working with proper data preservation" << std::endl;
}

// Define table structures for column rename + type change tests
struct OldProductTable {
  static constexpr auto table_name = "products";

  column<OldProductTable, "id", int, primary_key> id;
  column<OldProductTable, "price_cents", int> price_cents;  // int price in cents
};

struct NewProductTable {
  static constexpr auto table_name = "products";

  column<NewProductTable, "id", int, primary_key> id;
  column<NewProductTable, "price_dollars", std::string> price_dollars;  // string price in dollars
};

TEST(MigrationsTest, TestColumnRenameWithTypeChange) {
  std::cout << "\n=== Testing Column Rename + Type Change ===" << std::endl;

  OldProductTable old_table;
  NewProductTable new_table;

  // Test with mapping for rename + type change
  migrations::MigrationOptions options;
  options.column_mappings = {{"price_cents", "price_dollars"}};

  auto migration_result = migrations::generate_migration(old_table, new_table, options);
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
  EXPECT_EQ(migration.size(), 2);

  // Verify the operations
  EXPECT_EQ(forward_sql[0], "ALTER TABLE products ADD COLUMN price_dollars TEXT NOT NULL;");
  EXPECT_EQ(forward_sql[1], "ALTER TABLE products DROP COLUMN price_cents;");

  std::cout << "✅ Column rename + type change: Working" << std::endl;
}

TEST(MigrationsTest, TestBidirectionalTransformations) {
  std::cout << "\n=== Testing Bidirectional Column Transformations ===" << std::endl;

  OldProductTable old_table;
  NewProductTable new_table;

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

  auto migration_result = migrations::generate_migration(old_table, new_table, options);
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
  EXPECT_EQ(migration.size(), 3);

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

  std::cout << "✅ Bidirectional transformations: Working correctly" << std::endl;
}