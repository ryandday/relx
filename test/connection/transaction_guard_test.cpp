#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <relx/connection/transaction_guard.hpp>

namespace {

using relx::connection::Connection;
using relx::connection::ConnectionError;
using relx::connection::ConnectionResult;
using relx::connection::IsolationLevel;
using relx::connection::TransactionException;
using relx::connection::TransactionGuard;

/// Fake connection recording the transaction calls it receives; individual calls can
/// be made to fail to exercise the guard's error paths
class FakeConnection final : public Connection {
public:
  std::vector<std::string> calls;
  bool fail_begin = false;
  bool fail_commit = false;
  bool fail_rollback = false;

  ConnectionResult<void> connect() override { return {}; }
  ConnectionResult<void> disconnect() override { return {}; }

  ConnectionResult<relx::result::ResultSet> execute_raw(
      const std::string&, const std::vector<relx::bind_param>&) override {
    return relx::result::ResultSet{};
  }

  ConnectionResult<relx::result::ResultSet> execute_raw_binary_result(
      const std::string&, const std::vector<relx::bind_param>&) override {
    return relx::result::ResultSet{};
  }

  bool is_connected() const override { return true; }

  ConnectionResult<void> begin_transaction(IsolationLevel) override {
    calls.push_back("begin");
    if (fail_begin) {
      return std::unexpected(
          ConnectionError{.message = "begin failed", .error_code = 1, .sql_state = "40001"});
    }
    in_transaction_ = true;
    return {};
  }

  ConnectionResult<void> commit_transaction() override {
    calls.push_back("commit");
    if (fail_commit) {
      return std::unexpected(
          ConnectionError{.message = "commit failed", .error_code = 2, .sql_state = "40001"});
    }
    in_transaction_ = false;
    return {};
  }

  ConnectionResult<void> rollback_transaction() override {
    calls.push_back("rollback");
    if (fail_rollback) {
      return std::unexpected(ConnectionError{.message = "rollback failed", .error_code = 3});
    }
    in_transaction_ = false;
    return {};
  }

  bool in_transaction() const override { return in_transaction_; }

private:
  bool in_transaction_ = false;
};

TEST(TransactionGuardTest, CommitPreventsRollback) {
  FakeConnection conn;
  {
    TransactionGuard guard(conn);
    guard.commit();
    EXPECT_TRUE(guard.is_committed());
  }
  EXPECT_EQ(conn.calls, (std::vector<std::string>{"begin", "commit"}));
}

TEST(TransactionGuardTest, DestructorRollsBackUncommitted) {
  FakeConnection conn;
  {
    TransactionGuard guard(conn);
  }
  EXPECT_EQ(conn.calls, (std::vector<std::string>{"begin", "rollback"}));
}

TEST(TransactionGuardTest, RollbackOnThrow) {
  FakeConnection conn;
  EXPECT_THROW(
      {
        TransactionGuard guard(conn);
        throw std::runtime_error("boom");
      },
      std::runtime_error);
  EXPECT_EQ(conn.calls, (std::vector<std::string>{"begin", "rollback"}));
}

TEST(TransactionGuardTest, DoubleCommitThrows) {
  FakeConnection conn;
  TransactionGuard guard(conn);
  guard.commit();
  EXPECT_THROW(guard.commit(), TransactionException);
  EXPECT_THROW(guard.rollback(), TransactionException);
  // Only one commit reached the connection
  EXPECT_EQ(conn.calls, (std::vector<std::string>{"begin", "commit"}));
}

TEST(TransactionGuardTest, FailedBeginThrowsWithSqlState) {
  FakeConnection conn;
  conn.fail_begin = true;
  try {
    TransactionGuard guard(conn);
    FAIL() << "expected TransactionException";
  } catch (const TransactionException& e) {
    EXPECT_EQ(e.error_code(), 1);
    // The full ConnectionError travels through the guard, so retry loops can
    // classify the failure
    EXPECT_EQ(e.error().sql_state, "40001");
    EXPECT_TRUE(e.error().is_serialization_failure());
  }
}

TEST(TransactionGuardTest, FailedCommitThrowsAndDestructorDoesNotDoubleRollback) {
  FakeConnection conn;
  conn.fail_commit = true;
  {
    TransactionGuard guard(conn);
    try {
      guard.commit();
      FAIL() << "expected TransactionException";
    } catch (const TransactionException& e) {
      EXPECT_EQ(e.error().sql_state, "40001");
    }
  }
  // The failed commit left the transaction open, so the destructor rolls back
  EXPECT_EQ(conn.calls, (std::vector<std::string>{"begin", "commit", "rollback"}));
}

TEST(TransactionGuardTest, SwallowedRollbackFailureInDestructor) {
  FakeConnection conn;
  conn.fail_rollback = true;
  {
    TransactionGuard guard(conn);
  }  // rollback fails; the destructor must not throw
  EXPECT_EQ(conn.calls, (std::vector<std::string>{"begin", "rollback"}));
}

TEST(TransactionGuardTest, MoveTransfersResponsibility) {
  FakeConnection conn;
  {
    TransactionGuard guard(conn);
    TransactionGuard moved = std::move(guard);
    moved.commit();
  }
  // Exactly one commit, no rollback from the moved-from guard
  EXPECT_EQ(conn.calls, (std::vector<std::string>{"begin", "commit"}));
}

TEST(TransactionGuardTest, WithTransactionCommitsOnSuccess) {
  FakeConnection conn;
  TransactionGuard::with_transaction(conn, [](Connection&) {});
  EXPECT_EQ(conn.calls, (std::vector<std::string>{"begin", "commit"}));
}

TEST(TransactionGuardTest, WithTransactionRollsBackOnThrow) {
  FakeConnection conn;
  EXPECT_THROW(TransactionGuard::with_transaction(
                   conn, [](Connection&) { throw std::runtime_error("boom"); }),
               std::runtime_error);
  EXPECT_EQ(conn.calls, (std::vector<std::string>{"begin", "rollback"}));
}

}  // namespace
