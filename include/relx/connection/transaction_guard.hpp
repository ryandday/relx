#pragma once

#include "connection.hpp"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace relx {
namespace connection {

/// @brief Exception thrown when transaction operations fail
class TransactionException : public std::runtime_error {
public:
  explicit TransactionException(const ConnectionError& error)
      : std::runtime_error(error.message), error_code_(error.error_code) {}

  explicit TransactionException(const std::string& message)
      : std::runtime_error(message), error_code_(0) {}

  /// @brief Get the error code associated with this exception
  /// @return The database-specific error code
  int error_code() const noexcept { return error_code_; }

private:
  int error_code_;
};

/// @brief RAII wrapper for database transactions
/// @details Automatically begins a transaction on construction and either commits or
/// rolls back on destruction, depending on whether commit() was called or an error occurred.
class TransactionGuard {
public:
  /// @brief Constructor that begins a transaction
  /// @param connection The database connection to use
  /// @param isolation_level The isolation level for the transaction
  /// @throws TransactionException if the transaction cannot be started
  explicit TransactionGuard(Connection& connection,
                            IsolationLevel isolation_level = IsolationLevel::ReadCommitted)
      : connection_(connection), committed_(false), rolled_back_(false) {
    auto result = connection_.get().begin_transaction(isolation_level);
    if (!result) {
      throw TransactionException(result.error());
    }
  }

  /// @brief Destructor that rolls back the transaction if it wasn't committed or rolled back
  ~TransactionGuard() noexcept {
    if (!committed_ && !rolled_back_ && connection_.get().in_transaction()) {
      try {
        rollback();
      } catch (...) {
        // Suppress any exceptions from rollback in destructor
      }
    }
  }

  // Delete copy constructor and assignment operator
  TransactionGuard(const TransactionGuard&) = delete;
  TransactionGuard& operator=(const TransactionGuard&) = delete;

  // Allow move operations
  TransactionGuard(TransactionGuard&& other) noexcept
      : connection_(other.connection_), committed_(other.committed_),
        rolled_back_(other.rolled_back_) {
    // Mark the other transaction as rolled back to prevent double-rollback
    other.rolled_back_ = true;
  }

  TransactionGuard& operator=(TransactionGuard&& other) noexcept {
    if (this != &other) {
      // Rollback the current transaction if needed
      if (!committed_ && !rolled_back_ && connection_.get().in_transaction()) {
        try {
          rollback();
        } catch (...) {
          // Suppress any exceptions from rollback
        }
      }

      connection_ = other.connection_;
      committed_ = other.committed_;
      rolled_back_ = other.rolled_back_;

      // Mark the other transaction as rolled back to prevent double-rollback
      other.rolled_back_ = true;
    }
    return *this;
  }

  /// @brief Commit the transaction
  /// @throws TransactionException if the commit fails
  void commit() {
    if (committed_ || rolled_back_) {
      throw TransactionException("Transaction already committed or rolled back");
    }

    auto result = connection_.get().commit_transaction();
    if (!result) {
      throw TransactionException(result.error());
    }

    committed_ = true;
  }

  /// @brief Roll back the transaction
  /// @throws TransactionException if the rollback fails
  void rollback() {
    if (committed_ || rolled_back_) {
      throw TransactionException("Transaction already committed or rolled back");
    }

    auto result = connection_.get().rollback_transaction();
    if (!result) {
      throw TransactionException(result.error());
    }

    rolled_back_ = true;
  }

  /// @brief Check if the transaction has been committed
  /// @return True if the transaction was committed
  bool is_committed() const noexcept { return committed_; }

  /// @brief Check if the transaction has been rolled back
  /// @return True if the transaction was rolled back
  bool is_rolled_back() const noexcept { return rolled_back_; }

  /// @brief Execute a query within the transaction and commit on success
  /// @tparam Func A callable that executes database operations
  /// @param func The function to execute within the transaction
  /// @throws Any exception thrown by func, or TransactionException for transaction errors
  template <typename Func>
  static void with_transaction(Connection& connection, Func&& func,
                               IsolationLevel isolation_level = IsolationLevel::ReadCommitted) {
    TransactionGuard guard(connection, isolation_level);
    func(connection);
    guard.commit();
  }

private:
  std::reference_wrapper<Connection> connection_;
  bool committed_;
  bool rolled_back_;
};

}  // namespace connection

// Convenient imports from the connection namespace
using connection::TransactionException;
using connection::TransactionGuard;

}  // namespace relx