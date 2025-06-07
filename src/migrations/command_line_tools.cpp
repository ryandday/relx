#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>

#include <relx/migrations/command_line_tools.hpp>

namespace relx::migrations::cli {

void print_usage(const char* program_name, const std::vector<std::string>& supported_versions) {
  print_usage(program_name, supported_versions, true, true);
}

void print_usage(const char* program_name, const std::vector<std::string>& supported_versions,
                 bool create_available, bool drop_available) {
  std::cout << "Usage: " << program_name << " [options]\n"
            << "Options:\n"
            << "  --generate FROM TO [--output FILE]  Generate migration from version FROM to TO\n";

  if (create_available) {
    std::cout << "  --create VERSION [--output FILE]    Generate CREATE TABLE migration\n";
  }
  if (drop_available) {
    std::cout << "  --drop VERSION [--output FILE]      Generate DROP TABLE migration\n";
  }

  std::cout << "  --help                               Show this help\n\n";

  if (!supported_versions.empty()) {
    std::cout << "Supported versions: ";
    for (size_t i = 0; i < supported_versions.size(); ++i) {
      std::cout << supported_versions[i];
      if (i < supported_versions.size() - 1) std::cout << ", ";
    }
    std::cout << "\n\n";
  }

  std::cout << "Examples:\n"
            << "  " << program_name << " --generate v1 v2 --output migration_v1_to_v2.sql\n";

  if (create_available) {
    std::cout << "  " << program_name << " --create v2\n";
  }
  if (drop_available) {
    std::cout << "  " << program_name << " --drop v1 --output drop_users.sql\n";
  }
}

void write_migration_to_file(const Migration& migration, const std::string& filename,
                             bool include_rollback) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filename);
  }

  file << "-- Migration: " << migration.name() << "\n";
  file << "-- Generated at: " << std::chrono::system_clock::now() << "\n\n";

  file << "-- === FORWARD MIGRATION ===\n";
  auto forward_result = migration.forward_sql();
  if (!forward_result) {
    throw std::runtime_error("Failed to generate forward SQL: " + forward_result.error().format());
  }

  const auto& forward_sqls = *forward_result;
  for (size_t i = 0; i < forward_sqls.size(); ++i) {
    file << "-- Operation " << (i + 1) << "\n";
    file << forward_sqls[i] << "\n\n";
  }

  if (include_rollback) {
    file << "-- === ROLLBACK MIGRATION ===\n";
    file << "-- Uncomment the following to create a rollback script\n\n";

    auto rollback_result = migration.rollback_sql();
    if (!rollback_result) {
      throw std::runtime_error("Failed to generate rollback SQL: " +
                               rollback_result.error().format());
    }

    const auto& rollback_sqls = *rollback_result;
    for (size_t i = 0; i < rollback_sqls.size(); ++i) {
      file << "-- Rollback Operation " << (i + 1) << "\n";
      file << "-- " << rollback_sqls[i] << "\n\n";
    }
  }
}

void print_migration(const Migration& migration) {
  std::cout << "Migration: " << migration.name() << "\n";
  std::cout << "Operations: " << migration.size() << "\n\n";

  std::cout << "Forward Migration SQL:\n";
  auto forward_result = migration.forward_sql();
  if (!forward_result) {
    std::cerr << "Error generating forward SQL: " << forward_result.error().format() << "\n";
    return;
  }

  const auto& forward_sqls = *forward_result;
  for (size_t i = 0; i < forward_sqls.size(); ++i) {
    std::cout << (i + 1) << ". " << forward_sqls[i] << "\n";
  }

  std::cout << "\nRollback Migration SQL:\n";
  auto rollback_result = migration.rollback_sql();
  if (!rollback_result) {
    std::cerr << "Error generating rollback SQL: " << rollback_result.error().format() << "\n";
    return;
  }

  const auto& rollback_sqls = *rollback_result;
  for (size_t i = 0; i < rollback_sqls.size(); ++i) {
    std::cout << (i + 1) << ". " << rollback_sqls[i] << "\n";
  }
}

CommandLineArgs parse_args(const std::vector<std::string>& args) {
  CommandLineArgs result;

  if (args.empty()) {
    result.error_message = "No arguments provided";
    return result;
  }

  if (args[0] == "--help" || args[0] == "-h") {
    result.command = CommandLineArgs::Command::HELP;
    return result;
  } else if (args[0] == "--generate" || args[0] == "-g") {
    if (args.size() < 3) {
      result.error_message = "--generate requires FROM and TO versions";
      return result;
    }

    result.command = CommandLineArgs::Command::GENERATE;
    result.from_version = args[1];
    result.to_version = args[2];

    // Check for --output option
    for (size_t i = 3; i < args.size(); ++i) {
      if ((args[i] == "--output" || args[i] == "-o") && i + 1 < args.size()) {
        result.output_file = args[i + 1];
        break;
      } else if (args[i] == "--output" || args[i] == "-o") {
        result.command = CommandLineArgs::Command::INVALID;
        result.error_message = "--output requires filename";
        return result;
      }
    }
  } else if (args[0] == "--create") {
    if (args.size() < 2) {
      result.error_message = "--create requires version";
      return result;
    }

    result.command = CommandLineArgs::Command::CREATE;
    result.version = args[1];

    // Check for --output option
    for (size_t i = 2; i < args.size(); ++i) {
      if ((args[i] == "--output" || args[i] == "-o") && i + 1 < args.size()) {
        result.output_file = args[i + 1];
        break;
      } else if (args[i] == "--output" || args[i] == "-o") {
        result.command = CommandLineArgs::Command::INVALID;
        result.error_message = "--output requires filename";
        return result;
      }
    }
  } else if (args[0] == "--drop") {
    if (args.size() < 2) {
      result.error_message = "--drop requires version";
      return result;
    }

    result.command = CommandLineArgs::Command::DROP;
    result.version = args[1];

    // Check for --output option
    for (size_t i = 2; i < args.size(); ++i) {
      if ((args[i] == "--output" || args[i] == "-o") && i + 1 < args.size()) {
        result.output_file = args[i + 1];
        break;
      } else if (args[i] == "--output" || args[i] == "-o") {
        result.command = CommandLineArgs::Command::INVALID;
        result.error_message = "--output requires filename";
        return result;
      }
    }
  } else {
    result.command = CommandLineArgs::Command::INVALID;
    result.error_message = "Unknown command: " + args[0];
  }

  return result;
}

int run_migration_tool(int argc, char* argv[], const std::vector<std::string>& supported_versions,
                       MigrationGenerator migration_generator,
                       std::optional<CreateMigrationGenerator> create_generator,
                       std::optional<DropMigrationGenerator> drop_generator) {
  if (argc < 2) {
    bool create_available = create_generator.has_value();
    bool drop_available = drop_generator.has_value();
    print_usage(argv[0], supported_versions, create_available, drop_available);
    return 1;
  }

  std::vector<std::string> args(argv + 1, argv + argc);
  auto parsed_args = parse_args(args);

  bool create_available = create_generator.has_value();
  bool drop_available = drop_generator.has_value();

  switch (parsed_args.command) {
  case CommandLineArgs::Command::HELP:
    print_usage(argv[0], supported_versions, create_available, drop_available);
    return 0;

  case CommandLineArgs::Command::GENERATE: {
    // Validate supported versions
    bool from_supported = std::find(supported_versions.begin(), supported_versions.end(),
                                    parsed_args.from_version) != supported_versions.end();
    bool to_supported = std::find(supported_versions.begin(), supported_versions.end(),
                                  parsed_args.to_version) != supported_versions.end();

    if (!from_supported) {
      std::cerr << "Error: Unsupported version '" << parsed_args.from_version << "'\n";
      return 1;
    }
    if (!to_supported) {
      std::cerr << "Error: Unsupported version '" << parsed_args.to_version << "'\n";
      return 1;
    }

    auto migration_result = migration_generator(parsed_args.from_version, parsed_args.to_version);
    if (!migration_result) {
      std::cerr << "Error generating migration: " << migration_result.error().format() << "\n";
      return 1;
    }

    const auto& migration = *migration_result;
    if (!parsed_args.output_file.empty()) {
      write_migration_to_file(migration, parsed_args.output_file);
      std::cout << "Migration written to: " << parsed_args.output_file << "\n";
    } else {
      print_migration(migration);
    }
    return 0;
  }

  case CommandLineArgs::Command::CREATE: {
    if (!create_generator.has_value()) {
      std::cerr << "Error: CREATE table functionality is not available\n";
      return 1;
    }

    // Validate supported version
    bool version_supported = std::find(supported_versions.begin(), supported_versions.end(),
                                       parsed_args.version) != supported_versions.end();
    if (!version_supported) {
      std::cerr << "Error: Unsupported version '" << parsed_args.version << "'\n";
      return 1;
    }

    auto migration_result = (*create_generator)(parsed_args.version);
    if (!migration_result) {
      std::cerr << "Error generating create migration: " << migration_result.error().format()
                << "\n";
      return 1;
    }

    const auto& migration = *migration_result;
    if (!parsed_args.output_file.empty()) {
      write_migration_to_file(migration, parsed_args.output_file);
      std::cout << "Migration written to: " << parsed_args.output_file << "\n";
    } else {
      print_migration(migration);
    }
    return 0;
  }

  case CommandLineArgs::Command::DROP: {
    if (!drop_generator.has_value()) {
      std::cerr << "Error: DROP table functionality is not available\n";
      return 1;
    }

    // Validate supported version
    bool version_supported = std::find(supported_versions.begin(), supported_versions.end(),
                                       parsed_args.version) != supported_versions.end();
    if (!version_supported) {
      std::cerr << "Error: Unsupported version '" << parsed_args.version << "'\n";
      return 1;
    }

    auto migration_result = (*drop_generator)(parsed_args.version);
    if (!migration_result) {
      std::cerr << "Error generating drop migration: " << migration_result.error().format() << "\n";
      return 1;
    }

    const auto& migration = *migration_result;
    if (!parsed_args.output_file.empty()) {
      write_migration_to_file(migration, parsed_args.output_file);
      std::cout << "Migration written to: " << parsed_args.output_file << "\n";
    } else {
      print_migration(migration);
    }
    return 0;
  }

  case CommandLineArgs::Command::INVALID:
    std::cerr << "Error: " << parsed_args.error_message << "\n";
    print_usage(argv[0], supported_versions, create_available, drop_available);
    return 1;
  }

  return 1;
}

}  // namespace relx::migrations::cli