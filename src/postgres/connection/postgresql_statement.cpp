#include "relx/connection/postgresql_statement.hpp"

#include <iostream>

namespace relx::connection {

PostgreSQLStatement::PostgreSQLStatement(PostgreSQLConnection& connection, std::string name,
                                         std::string sql, int param_count)
    : connection_(&connection), name_(std::move(name)), sql_(std::move(sql)),
      param_count_(param_count) {
  // The statement should already be prepared by the PostgreSQLConnection class
  // We just initialize the object here
}

PostgreSQLStatement::~PostgreSQLStatement() {
  if (is_valid_) {
    // Deallocate the prepared statement
    auto result = connection_->execute_raw("DEALLOCATE " + name_);
    if (!result) {
      // TODO more customizable user behavior for this
      std::cerr << "Failed to deallocate statement: " << result.error().message << std::endl;
    }
  }
}

PostgreSQLStatement::PostgreSQLStatement(PostgreSQLStatement&& other) noexcept
    : connection_(other.connection_), name_(std::move(other.name_)), sql_(std::move(other.sql_)),
      param_count_(other.param_count_), is_valid_(other.is_valid_) {
  // Mark the moved-from object as invalid
  other.is_valid_ = false;
}

PostgreSQLStatement& PostgreSQLStatement::operator=(PostgreSQLStatement&& other) noexcept {
  if (this != &other) {
    // Clean up this object
    if (is_valid_) {
      auto result = connection_->execute_raw("DEALLOCATE " + name_);
      if (!result) {
        std::cerr << "Failed to deallocate statement: " << result.error().message << std::endl;
      }
    }

    // Move data from other
    connection_ = other.connection_;
    name_ = std::move(other.name_);
    sql_ = std::move(other.sql_);
    param_count_ = other.param_count_;
    is_valid_ = other.is_valid_;

    // Mark the moved-from object as invalid
    other.is_valid_ = false;
  }
  return *this;
}

ConnectionResult<result::ResultSet> PostgreSQLStatement::execute(
    const std::vector<std::optional<std::string>>& params) {
  if (!is_valid_) {
    return std::unexpected(ConnectionError{.message = "Statement is not valid", .error_code = -1});
  }

  if (params.size() != static_cast<size_t>(param_count_)) {
    return std::unexpected(
        ConnectionError{.message = "Parameter count mismatch", .error_code = -1});
  }

  return connection_->execute_prepared(name_, params);
}

}  // namespace relx::connection
