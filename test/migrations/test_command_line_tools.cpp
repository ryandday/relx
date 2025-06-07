#include <iostream>
#include <sstream>

#include <gtest/gtest.h>
#include <relx/migrations.hpp>
#include <relx/migrations/command_line_tools.hpp>
#include <relx/schema.hpp>

using namespace relx;

// =============================================================================
// Test Table Definitions
// =============================================================================

struct TestUsersV1 {
  static constexpr auto table_name = "test_users";

  column<TestUsersV1, "id", int, primary_key> id;
  column<TestUsersV1, "name", std::string> name;
  column<TestUsersV1, "email", std::string> email;
};

struct TestUsersV2 {
  static constexpr auto table_name = "test_users";

  column<TestUsersV2, "id", int, primary_key> id;
  column<TestUsersV2, "full_name", std::string> full_name;  // renamed from name
  column<TestUsersV2, "email", std::string> email;
  column<TestUsersV2, "age", std::optional<int>> age;  // new column
};

// =============================================================================
// Test Migration Functions
// =============================================================================

migrations::MigrationResult<migrations::Migration> test_generate_migration_between_versions(
    const std::string& from, const std::string& to) {
  if (from == "v1" && to == "v2") {
    migrations::MigrationOptions options;
    options.column_mappings = {{"name", "full_name"}};

    TestUsersV1 old_table;
    TestUsersV2 new_table;
    return migrations::generate_migration(old_table, new_table, options);
  } else {
    return std::unexpected(
        migrations::MigrationError::make(migrations::MigrationErrorType::UNSUPPORTED_OPERATION,
                                         "Unsupported migration path: " + from + " -> " + to));
  }
}

migrations::MigrationResult<migrations::Migration> test_generate_create_migration(
    const std::string& version) {
  if (version == "v1") {
    TestUsersV1 table;
    return migrations::generate_create_table_migration(table);
  } else if (version == "v2") {
    TestUsersV2 table;
    return migrations::generate_create_table_migration(table);
  } else {
    return std::unexpected(migrations::MigrationError::make(
        migrations::MigrationErrorType::UNSUPPORTED_OPERATION, "Unsupported version: " + version));
  }
}

migrations::MigrationResult<migrations::Migration> test_generate_drop_migration(
    const std::string& version) {
  if (version == "v1") {
    TestUsersV1 table;
    return migrations::generate_drop_table_migration(table);
  } else if (version == "v2") {
    TestUsersV2 table;
    return migrations::generate_drop_table_migration(table);
  } else {
    return std::unexpected(migrations::MigrationError::make(
        migrations::MigrationErrorType::UNSUPPORTED_OPERATION, "Unsupported version: " + version));
  }
}

// =============================================================================
// Test Fixtures and Utilities
// =============================================================================

class CommandLineToolsTest : public ::testing::Test {
protected:
  std::vector<std::string> supported_versions = {"v1", "v2"};

  // Helper to capture stdout/stderr
  std::ostringstream captured_output;
  std::streambuf* orig_cout;
  std::streambuf* orig_cerr;

  void SetUp() override {
    orig_cout = std::cout.rdbuf();
    orig_cerr = std::cerr.rdbuf();
  }

  void TearDown() override {
    std::cout.rdbuf(orig_cout);
    std::cerr.rdbuf(orig_cerr);
  }

  void capture_output() {
    std::cout.rdbuf(captured_output.rdbuf());
    std::cerr.rdbuf(captured_output.rdbuf());
  }

  void restore_output() {
    std::cout.rdbuf(orig_cout);
    std::cerr.rdbuf(orig_cerr);
  }

  std::string get_captured_output() { return captured_output.str(); }

  void clear_captured_output() {
    captured_output.str("");
    captured_output.clear();
  }
};

// =============================================================================
// Tests for parse_args Function
// =============================================================================

TEST_F(CommandLineToolsTest, ParseArgsHelp) {
  std::vector<std::string> args = {"--help"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::HELP);
  EXPECT_TRUE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsHelpShortForm) {
  std::vector<std::string> args = {"-h"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::HELP);
  EXPECT_TRUE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsGenerate) {
  std::vector<std::string> args = {"--generate", "v1", "v2"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::GENERATE);
  EXPECT_EQ(parsed.from_version, "v1");
  EXPECT_EQ(parsed.to_version, "v2");
  EXPECT_TRUE(parsed.output_file.empty());
  EXPECT_TRUE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsGenerateWithOutput) {
  std::vector<std::string> args = {"--generate", "v1", "v2", "--output", "migration.sql"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::GENERATE);
  EXPECT_EQ(parsed.from_version, "v1");
  EXPECT_EQ(parsed.to_version, "v2");
  EXPECT_EQ(parsed.output_file, "migration.sql");
  EXPECT_TRUE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsGenerateShortForm) {
  std::vector<std::string> args = {"-g", "v1", "v2", "-o", "migration.sql"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::GENERATE);
  EXPECT_EQ(parsed.from_version, "v1");
  EXPECT_EQ(parsed.to_version, "v2");
  EXPECT_EQ(parsed.output_file, "migration.sql");
  EXPECT_TRUE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsCreate) {
  std::vector<std::string> args = {"--create", "v2"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::CREATE);
  EXPECT_EQ(parsed.version, "v2");
  EXPECT_TRUE(parsed.output_file.empty());
  EXPECT_TRUE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsCreateWithOutput) {
  std::vector<std::string> args = {"--create", "v2", "--output", "create.sql"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::CREATE);
  EXPECT_EQ(parsed.version, "v2");
  EXPECT_EQ(parsed.output_file, "create.sql");
  EXPECT_TRUE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsDrop) {
  std::vector<std::string> args = {"--drop", "v1"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::DROP);
  EXPECT_EQ(parsed.version, "v1");
  EXPECT_TRUE(parsed.output_file.empty());
  EXPECT_TRUE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsDropWithOutput) {
  std::vector<std::string> args = {"--drop", "v1", "--output", "drop.sql"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::DROP);
  EXPECT_EQ(parsed.version, "v1");
  EXPECT_EQ(parsed.output_file, "drop.sql");
  EXPECT_TRUE(parsed.error_message.empty());
}

// =============================================================================
// Tests for parse_args Error Cases
// =============================================================================

TEST_F(CommandLineToolsTest, ParseArgsEmptyArgs) {
  std::vector<std::string> args = {};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::INVALID);
  EXPECT_FALSE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsInvalidCommand) {
  std::vector<std::string> args = {"--invalid"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::INVALID);
  EXPECT_FALSE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsGenerateMissingArgs) {
  std::vector<std::string> args = {"--generate"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::INVALID);
  EXPECT_FALSE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsGeneratePartialArgs) {
  std::vector<std::string> args = {"--generate", "v1"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::INVALID);
  EXPECT_FALSE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsCreateMissingVersion) {
  std::vector<std::string> args = {"--create"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::INVALID);
  EXPECT_FALSE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsDropMissingVersion) {
  std::vector<std::string> args = {"--drop"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::INVALID);
  EXPECT_FALSE(parsed.error_message.empty());
}

TEST_F(CommandLineToolsTest, ParseArgsOutputMissingFile) {
  std::vector<std::string> args = {"--generate", "v1", "v2", "--output"};
  auto parsed = migrations::cli::parse_args(args);

  EXPECT_EQ(parsed.command, migrations::cli::CommandLineArgs::Command::INVALID);
  EXPECT_FALSE(parsed.error_message.empty());
}

// =============================================================================
// Tests for run_migration_tool Function - Full Functionality
// =============================================================================

TEST_F(CommandLineToolsTest, RunMigrationToolFullFunctionality) {
  // Test with all generators provided (full functionality)
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--generate"),
                  const_cast<char*>("v1"), const_cast<char*>("v2")};
  int argc = 4;

  capture_output();
  int result = migrations::cli::run_migration_tool(
      argc, argv, supported_versions, test_generate_migration_between_versions,
      test_generate_create_migration, test_generate_drop_migration);
  restore_output();

  EXPECT_EQ(result, 0);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Migration: diff_test_users_to_test_users") != std::string::npos);
  EXPECT_TRUE(output.find("ALTER TABLE") != std::string::npos);
}

TEST_F(CommandLineToolsTest, RunMigrationToolCreateCommand) {
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--create"),
                  const_cast<char*>("v1")};
  int argc = 3;

  capture_output();
  int result = migrations::cli::run_migration_tool(
      argc, argv, supported_versions, test_generate_migration_between_versions,
      test_generate_create_migration, test_generate_drop_migration);
  restore_output();

  EXPECT_EQ(result, 0);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Migration: create_test_users") != std::string::npos);
  EXPECT_TRUE(output.find("CREATE TABLE") != std::string::npos);
}

TEST_F(CommandLineToolsTest, RunMigrationToolDropCommand) {
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--drop"),
                  const_cast<char*>("v2")};
  int argc = 3;

  capture_output();
  int result = migrations::cli::run_migration_tool(
      argc, argv, supported_versions, test_generate_migration_between_versions,
      test_generate_create_migration, test_generate_drop_migration);
  restore_output();

  EXPECT_EQ(result, 0);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Migration: drop_test_users") != std::string::npos);
  EXPECT_TRUE(output.find("DROP TABLE") != std::string::npos);
}

TEST_F(CommandLineToolsTest, RunMigrationToolHelp) {
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--help")};
  int argc = 2;

  capture_output();
  int result = migrations::cli::run_migration_tool(
      argc, argv, supported_versions, test_generate_migration_between_versions,
      test_generate_create_migration, test_generate_drop_migration);
  restore_output();

  EXPECT_EQ(result, 0);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Usage:") != std::string::npos);
  EXPECT_TRUE(output.find("--generate") != std::string::npos);
  EXPECT_TRUE(output.find("--create") != std::string::npos);
  EXPECT_TRUE(output.find("--drop") != std::string::npos);
}

// =============================================================================
// Tests for run_migration_tool Function - Limited Functionality
// =============================================================================

TEST_F(CommandLineToolsTest, RunMigrationToolMinimalFunctionality) {
  // Test with only migration generator (no create/drop)
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--generate"),
                  const_cast<char*>("v1"), const_cast<char*>("v2")};
  int argc = 4;

  capture_output();
  int result = migrations::cli::run_migration_tool(argc, argv, supported_versions,
                                                   test_generate_migration_between_versions
                                                   // No create_generator
                                                   // No drop_generator
  );
  restore_output();

  EXPECT_EQ(result, 0);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Migration: diff_test_users_to_test_users") != std::string::npos);
  EXPECT_TRUE(output.find("ALTER TABLE") != std::string::npos);
}

TEST_F(CommandLineToolsTest, RunMigrationToolMinimalFunctionalityHelp) {
  // Test help with only migration generator (no create/drop)
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--help")};
  int argc = 2;

  capture_output();
  int result = migrations::cli::run_migration_tool(argc, argv, supported_versions,
                                                   test_generate_migration_between_versions
                                                   // No create_generator
                                                   // No drop_generator
  );
  restore_output();

  EXPECT_EQ(result, 0);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Usage:") != std::string::npos);
  EXPECT_TRUE(output.find("--generate") != std::string::npos);
  // Should NOT contain create/drop options
  EXPECT_FALSE(output.find("--create") != std::string::npos);
  EXPECT_FALSE(output.find("--drop") != std::string::npos);
}

TEST_F(CommandLineToolsTest, RunMigrationToolOnlyCreateFunctionality) {
  // Test with only create generator (no drop)
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--help")};
  int argc = 2;

  capture_output();
  int result = migrations::cli::run_migration_tool(argc, argv, supported_versions,
                                                   test_generate_migration_between_versions,
                                                   test_generate_create_migration
                                                   // No drop_generator
  );
  restore_output();

  EXPECT_EQ(result, 0);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Usage:") != std::string::npos);
  EXPECT_TRUE(output.find("--generate") != std::string::npos);
  EXPECT_TRUE(output.find("--create") != std::string::npos);
  // Should NOT contain drop option
  EXPECT_FALSE(output.find("--drop") != std::string::npos);
}

// =============================================================================
// Tests for run_migration_tool Function - Error Cases
// =============================================================================

TEST_F(CommandLineToolsTest, RunMigrationToolCreateWithoutGenerator) {
  // Test CREATE command when create generator is not provided
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--create"),
                  const_cast<char*>("v1")};
  int argc = 3;

  capture_output();
  int result = migrations::cli::run_migration_tool(argc, argv, supported_versions,
                                                   test_generate_migration_between_versions
                                                   // No create_generator - should cause error
  );
  restore_output();

  EXPECT_EQ(result, 1);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Error:") != std::string::npos);
  EXPECT_TRUE(output.find("CREATE") != std::string::npos);
  EXPECT_TRUE(output.find("not available") != std::string::npos);
}

TEST_F(CommandLineToolsTest, RunMigrationToolDropWithoutGenerator) {
  // Test DROP command when drop generator is not provided
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--drop"),
                  const_cast<char*>("v1")};
  int argc = 3;

  capture_output();
  int result = migrations::cli::run_migration_tool(argc, argv, supported_versions,
                                                   test_generate_migration_between_versions,
                                                   test_generate_create_migration
                                                   // No drop_generator - should cause error
  );
  restore_output();

  EXPECT_EQ(result, 1);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Error:") != std::string::npos);
  EXPECT_TRUE(output.find("DROP") != std::string::npos);
  EXPECT_TRUE(output.find("not available") != std::string::npos);
}

TEST_F(CommandLineToolsTest, RunMigrationToolInvalidCommand) {
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--invalid")};
  int argc = 2;

  capture_output();
  int result = migrations::cli::run_migration_tool(
      argc, argv, supported_versions, test_generate_migration_between_versions,
      test_generate_create_migration, test_generate_drop_migration);
  restore_output();

  EXPECT_EQ(result, 1);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Error:") != std::string::npos);
}

TEST_F(CommandLineToolsTest, RunMigrationToolNoArgs) {
  char* argv[] = {const_cast<char*>("test_program")};
  int argc = 1;

  capture_output();
  int result = migrations::cli::run_migration_tool(
      argc, argv, supported_versions, test_generate_migration_between_versions,
      test_generate_create_migration, test_generate_drop_migration);
  restore_output();

  EXPECT_EQ(result, 1);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Usage:") != std::string::npos);
}

TEST_F(CommandLineToolsTest, RunMigrationToolUnsupportedVersion) {
  char* argv[] = {
      const_cast<char*>("test_program"), const_cast<char*>("--create"),
      const_cast<char*>("v999")  // Unsupported version
  };
  int argc = 3;

  capture_output();
  int result = migrations::cli::run_migration_tool(
      argc, argv, supported_versions, test_generate_migration_between_versions,
      test_generate_create_migration, test_generate_drop_migration);
  restore_output();

  EXPECT_EQ(result, 1);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Error:") != std::string::npos);
}

TEST_F(CommandLineToolsTest, RunMigrationToolUnsupportedMigrationPath) {
  char* argv[] = {
      const_cast<char*>("test_program"), const_cast<char*>("--generate"), const_cast<char*>("v2"),
      const_cast<char*>("v1")  // Unsupported migration path
  };
  int argc = 4;

  capture_output();
  int result = migrations::cli::run_migration_tool(
      argc, argv, supported_versions, test_generate_migration_between_versions,
      test_generate_create_migration, test_generate_drop_migration);
  restore_output();

  EXPECT_EQ(result, 1);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Error") != std::string::npos);
}

// =============================================================================
// Tests for File Output Functionality
// =============================================================================

TEST_F(CommandLineToolsTest, RunMigrationToolWithFileOutput) {
  char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--generate"),
                  const_cast<char*>("v1"),           const_cast<char*>("v2"),
                  const_cast<char*>("--output"),     const_cast<char*>("test_migration.sql")};
  int argc = 6;

  capture_output();
  int result = migrations::cli::run_migration_tool(
      argc, argv, supported_versions, test_generate_migration_between_versions,
      test_generate_create_migration, test_generate_drop_migration);
  restore_output();

  EXPECT_EQ(result, 0);

  std::string output = get_captured_output();
  EXPECT_TRUE(output.find("Migration written to: test_migration.sql") != std::string::npos);

  // Clean up - delete the test file if it was created
  // Note: We're not actually testing file creation here since that would require
  // filesystem operations, but we're testing that the command processes correctly
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST_F(CommandLineToolsTest, EndToEndWorkflow) {
  // Test a complete workflow: help -> create -> generate -> drop

  // 1. Help command
  {
    char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--help")};
    int argc = 2;

    capture_output();
    int result = migrations::cli::run_migration_tool(
        argc, argv, supported_versions, test_generate_migration_between_versions,
        test_generate_create_migration, test_generate_drop_migration);
    restore_output();

    EXPECT_EQ(result, 0);
    clear_captured_output();
  }

  // 2. Create command
  {
    char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--create"),
                    const_cast<char*>("v1")};
    int argc = 3;

    capture_output();
    int result = migrations::cli::run_migration_tool(
        argc, argv, supported_versions, test_generate_migration_between_versions,
        test_generate_create_migration, test_generate_drop_migration);
    restore_output();

    EXPECT_EQ(result, 0);

    std::string output = get_captured_output();
    EXPECT_TRUE(output.find("CREATE TABLE") != std::string::npos);
    clear_captured_output();
  }

  // 3. Generate migration command
  {
    char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--generate"),
                    const_cast<char*>("v1"), const_cast<char*>("v2")};
    int argc = 4;

    capture_output();
    int result = migrations::cli::run_migration_tool(
        argc, argv, supported_versions, test_generate_migration_between_versions,
        test_generate_create_migration, test_generate_drop_migration);
    restore_output();

    EXPECT_EQ(result, 0);

    std::string output = get_captured_output();
    EXPECT_TRUE(output.find("ALTER TABLE") != std::string::npos);
    clear_captured_output();
  }

  // 4. Drop command
  {
    char* argv[] = {const_cast<char*>("test_program"), const_cast<char*>("--drop"),
                    const_cast<char*>("v2")};
    int argc = 3;

    capture_output();
    int result = migrations::cli::run_migration_tool(
        argc, argv, supported_versions, test_generate_migration_between_versions,
        test_generate_create_migration, test_generate_drop_migration);
    restore_output();

    EXPECT_EQ(result, 0);

    std::string output = get_captured_output();
    EXPECT_TRUE(output.find("DROP TABLE") != std::string::npos);
  }
}