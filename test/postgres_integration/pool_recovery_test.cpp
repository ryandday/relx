#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/connection/postgresql_connection.hpp>
#include <relx/connection/postgresql_connection_pool.hpp>

// Failover/recovery: every pooled connection goes stale at once (as it would when the
// database restarts), and the pool must detect the dead connections during validation
// and hand out fresh working ones instead of deadlocking or returning corpses.

namespace {

using relx::connection::PostgreSQLConnectionParams;
using relx::connection::PostgreSQLConnectionPool;
using relx::connection::PostgreSQLConnectionPoolConfig;

PostgreSQLConnectionParams test_params() {
  PostgreSQLConnectionParams params;
  params.host = "localhost";
  params.port = 5434;
  params.dbname = "relx_test";
  params.user = "postgres";
  params.password = "postgres";
  return params;
}

TEST(PoolRecoveryTest, SurvivesAllBackendsTerminated) {
  PostgreSQLConnectionPoolConfig config;
  config.connection_params = test_params();
  config.initial_size = 3;
  config.max_size = 5;
  config.validate_connections = true;

  auto pool = PostgreSQLConnectionPool::create(config);
  auto initialized = pool->initialize();
  ASSERT_TRUE(initialized) << initialized.error().message;

  // Collect the backend pids of the pooled connections
  std::vector<std::string> pids;
  {
    std::vector<PostgreSQLConnectionPool::PooledConnection> held;
    for (int i = 0; i < 3; ++i) {
      auto conn = pool->get_connection();
      ASSERT_TRUE(conn) << conn.error().message;
      auto pid = (*conn)->execute_raw("SELECT pg_backend_pid()");
      ASSERT_TRUE(pid) << pid.error().message;
      pids.push_back(pid->at(0).get_cell(0).value()->raw_value());
      held.push_back(std::move(*conn));
    }
    // held goes out of scope: all three connections return to the pool
  }

  // From a separate connection, kill every pooled backend - the moral equivalent
  // of a server restart for the pool's idle connections
  relx::connection::PostgreSQLConnection admin(test_params());
  ASSERT_TRUE(admin.connect());
  for (const auto& pid : pids) {
    auto terminated = admin.execute_raw("SELECT pg_terminate_backend(" + pid + ")");
    ASSERT_TRUE(terminated) << terminated.error().message;
  }

  // The pool must notice the dead connections and produce working replacements
  for (int i = 0; i < 3; ++i) {
    auto conn = pool->get_connection();
    ASSERT_TRUE(conn) << conn.error().message;
    auto result = (*conn)->execute_raw("SELECT 1");
    EXPECT_TRUE(result) << (result ? "" : result.error().message);
  }

  ASSERT_TRUE(admin.disconnect());
}

}  // namespace
