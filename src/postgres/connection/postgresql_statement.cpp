#include "relx/connection/postgresql_statement.hpp"

namespace relx::connection {

PostgreSQLStatement::PostgreSQLStatement(PostgreSQLConnection& connection, std::string name,
                                         std::string sql, int param_count)
    : connection_(&connection), name_(std::move(name)), sql_(std::move(sql)),
      param_count_(param_count) {
  // The statement is already prepared server-side; register so the connection can
  // rebind this object on moves and invalidate it on disconnect/destruction
  connection_->register_statement(this);
}

PostgreSQLStatement::~PostgreSQLStatement() {
  if (connection_ != nullptr) {
    if (is_valid_) {
      // Deallocate the prepared statement. A destructor has no error channel, and a
      // library must not write to stderr; a failed DEALLOCATE is resolved when the
      // connection closes anyway.
      [[maybe_unused]] auto result = connection_->execute_raw("DEALLOCATE " + name_);
    }
    connection_->deregister_statement(this);
  }
}

PostgreSQLStatement::PostgreSQLStatement(PostgreSQLStatement&& other) noexcept
    : connection_(other.connection_), name_(std::move(other.name_)), sql_(std::move(other.sql_)),
      param_count_(other.param_count_), is_valid_(other.is_valid_) {
  // Take over the registration; the moved-from object detaches entirely
  if (connection_ != nullptr) {
    connection_->deregister_statement(&other);
    connection_->register_statement(this);
  }
  other.connection_ = nullptr;
  other.is_valid_ = false;
}

PostgreSQLStatement& PostgreSQLStatement::operator=(PostgreSQLStatement&& other) noexcept {
  if (this != &other) {
    // Clean up this object
    if (connection_ != nullptr) {
      if (is_valid_) {
        // See the destructor: no error channel here, and no stderr from library code
        [[maybe_unused]] auto result = connection_->execute_raw("DEALLOCATE " + name_);
      }
      connection_->deregister_statement(this);
    }

    // Move data from other and take over its registration
    connection_ = other.connection_;
    name_ = std::move(other.name_);
    sql_ = std::move(other.sql_);
    param_count_ = other.param_count_;
    is_valid_ = other.is_valid_;
    if (connection_ != nullptr) {
      connection_->deregister_statement(&other);
      connection_->register_statement(this);
    }

    // The moved-from object detaches entirely
    other.connection_ = nullptr;
    other.is_valid_ = false;
  }
  return *this;
}

ConnectionResult<result::ResultSet> PostgreSQLStatement::execute(
    const std::vector<std::optional<std::string>>& params) {
  if (connection_ == nullptr) {
    return std::unexpected(ConnectionError{
        .message = "Statement's connection has been disconnected or destroyed", .error_code = -1});
  }
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
