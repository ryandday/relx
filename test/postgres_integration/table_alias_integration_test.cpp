#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Table aliases against a real PostgreSQL: self-joins via relx::t<T, "alias">

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("tai_employees")]] Employee {
  [[=relx::ann::pk]] int id;
  std::string name;
  // Self-referential FK: annotations cannot name the struct's own members, so the
  // reference is spelled with the classic references<> modifier
  [[=relx::schema::references<"tai_employees", "id">{}]] std::optional<int> manager_id;
};
inline constexpr auto emp = relx::t<Employee>;
inline constexpr auto mgr = relx::t<Employee, "m">;

// clang-format on

class TableAliasIntegrationTest : public ::testing::Test {
protected:
  std::string conn_string =
      "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
  std::unique_ptr<relx::PostgreSQLConnection> conn;

  void SetUp() override {
    conn = std::make_unique<relx::PostgreSQLConnection>(conn_string);
    auto connect_result = conn->connect();
    ASSERT_TRUE(connect_result) << "Failed to connect: " << connect_result.error().message;

    ASSERT_TRUE(conn->execute_raw("DROP TABLE IF EXISTS tai_employees CASCADE;"));
    ASSERT_TRUE(conn->execute_raw(std::string(relx::create_table_sql<Employee>().to_sql())));

    // Carol manages Ada and Bob; Carol has no manager
    const Employee carol{.id = 1, .name = "Carol", .manager_id = std::nullopt};
    const Employee ada{.id = 2, .name = "Ada", .manager_id = 1};
    const Employee bob{.id = 3, .name = "Bob", .manager_id = 1};
    ASSERT_TRUE(conn->execute(relx::insert_into(emp).values_from(carol, ada, bob)));
  }

  void TearDown() override {
    if (conn && conn->is_connected()) {
      ASSERT_TRUE(conn->execute_raw("DROP TABLE IF EXISTS tai_employees CASCADE;"));
      auto disconnect_result = conn->disconnect();
      EXPECT_TRUE(disconnect_result) << disconnect_result.error().message;
    }
  }
};

}  // namespace

TEST_F(TableAliasIntegrationTest, SelfJoinResolvesManagers) {
  auto query = relx::query::select(emp.name, relx::as<"manager_name">(mgr.name))
                   .from(emp)
                   .join(mgr, relx::query::on(emp.manager_id == mgr.id))
                   .order_by(relx::query::asc(emp.id));
  auto rows = conn->fetch_all(query);
  ASSERT_TRUE(rows) << rows.error().message;
  ASSERT_EQ(rows->size(), 2);
  EXPECT_EQ(rows->front().name, "Ada");
  EXPECT_EQ(rows->front().manager_name, "Carol");
  EXPECT_EQ(rows->back().name, "Bob");
}

TEST_F(TableAliasIntegrationTest, LeftSelfJoinNestsOptionalAliasMember) {
  auto query = relx::query::select(emp, mgr)
                   .from(emp)
                   .left_join(mgr, relx::query::on(emp.manager_id == mgr.id))
                   .order_by(relx::query::asc(emp.id));
  auto rows = conn->fetch_all(query);
  ASSERT_TRUE(rows) << rows.error().message;
  ASSERT_EQ(rows->size(), 3);

  // Carol: no manager, aliased side is nullopt
  EXPECT_EQ(rows->front().tai_employees.name, "Carol");
  EXPECT_FALSE(rows->front().m.has_value());

  // Ada: managed by Carol
  ASSERT_TRUE((*rows)[1].m.has_value());
  EXPECT_EQ((*rows)[1].m->name, "Carol");
}

TEST_F(TableAliasIntegrationTest, WhereOnAliasedColumns) {
  auto query = relx::query::select(emp.id, emp.name)
                   .from(emp)
                   .join(mgr, relx::query::on(emp.manager_id == mgr.id))
                   .where(mgr.name == "Carol")
                   .order_by(relx::query::asc(emp.id));
  auto rows = conn->fetch_all(query);
  ASSERT_TRUE(rows) << rows.error().message;
  EXPECT_EQ(rows->size(), 2);
}
