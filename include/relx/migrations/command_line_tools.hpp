#pragma once

#include "core.hpp"

#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace relx::migrations::cli {

/// @brief Print usage information for a migration command-line tool
/// @param program_name The name of the program executable
/// @param supported_versions List of supported version strings (e.g., {"v1", "v2", "v3"})
void print_usage(const char* program_name, const std::vector<std::string>& supported_versions = {});

/// @brief Print usage information for a migration command-line tool with customizable options
/// @param program_name The name of the program executable
/// @param supported_versions List of supported version strings (e.g., {"v1", "v2", "v3"})
/// @param create_available Whether CREATE table functionality is available
/// @param drop_available Whether DROP table functionality is available
void print_usage(const char* program_name, const std::vector<std::string>& supported_versions,
                 bool create_available, bool drop_available);

/// @brief Write a migration to a SQL file with proper formatting
/// @param migration The migration to write
/// @param filename The output filename
/// @param include_rollback Whether to include rollback SQL as comments (default: true)
void write_migration_to_file(const Migration& migration, const std::string& filename,
                             bool include_rollback = true);

/// @brief Print a migration to the console with formatting
/// @param migration The migration to print
void print_migration(const Migration& migration);

/// @brief Command-line argument parsing result
struct CommandLineArgs {
  enum class Command { HELP, GENERATE, CREATE, DROP, INVALID };

  Command command = Command::INVALID;
  std::string from_version;
  std::string to_version;
  std::string version;
  std::string output_file;
  std::string error_message;
};

/// @brief Parse command-line arguments for migration tools
/// @param args Command-line arguments (excluding program name)
/// @return Parsed arguments structure
CommandLineArgs parse_args(const std::vector<std::string>& args);

/// @brief Type alias for a function that generates a migration between two versions
/// @param from_version The starting version
/// @param to_version The target version
/// @return Migration object containing the necessary operations
using MigrationGenerator =
    std::function<MigrationResult<Migration>(const std::string&, const std::string&)>;

/// @brief Type alias for a function that generates a create table migration
/// @param version The version to create
/// @return Migration object containing the create table operation
using CreateMigrationGenerator = std::function<MigrationResult<Migration>(const std::string&)>;

/// @brief Type alias for a function that generates a drop table migration
/// @param version The version to drop
/// @return Migration object containing the drop table operation
using DropMigrationGenerator = std::function<MigrationResult<Migration>(const std::string&)>;

/// @brief Run a complete migration command-line tool
/// @param argc Command-line argument count
/// @param argv Command-line argument values
/// @param supported_versions List of supported version strings
/// @param migration_generator Function to generate migrations between versions
/// @param create_generator Optional function to generate create table migrations
/// @param drop_generator Optional function to generate drop table migrations
/// @return Exit code (0 for success, 1 for error)
int run_migration_tool(int argc, char* argv[], const std::vector<std::string>& supported_versions,
                       MigrationGenerator migration_generator,
                       std::optional<CreateMigrationGenerator> create_generator = std::nullopt,
                       std::optional<DropMigrationGenerator> drop_generator = std::nullopt);

}  // namespace relx::migrations::cli