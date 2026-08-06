#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Struct-based writes against a real PostgreSQL: values_from, upsert, set_from

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

enum class Tier { basic, premium };

struct [[=relx::table("sw_accounts")]] Account {
  [[=relx::ann::pk, =relx::ann::identity]] int id;
  [[=relx::ann::unique]] std::string email;
  std::string name;
  bool active;
  Tier tier;
  std::optional<std::string> note;
};
inline constexpr auto accounts = relx::t<Account>;

struct [[=relx::table("sw_kv")]] KeyValue {
  [[=relx::ann::pk]] std::string key;
  std::string value;
};
inline constexpr auto kv = relx::t<KeyValue>;

struct [[=relx::table("sw_grid"), =relx::ann::composite_pk("row", "col")]] GridCell {
  int row;
  int col;
  std::string content;
};
inline constexpr auto grid = relx::t<GridCell>;

struct [[=relx::table("sw_edges"), =relx::ann::composite_pk("src", "dst")]] Edge {
  int src;
  int dst;
};
inline constexpr auto edges = relx::t<Edge>;

struct AccountPatch {
  std::optional<std::string> name;
  std::optional<bool> active;
  std::optional<std::string> note;
};

// clang-format on

class StructWritesIntegrationTest : public ::testing::Test {
protected:
  std::string conn_string =
      "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
  std::unique_ptr<relx::PostgreSQLConnection> conn;

  void SetUp() override {
    conn = std::make_unique<relx::PostgreSQLConnection>(conn_string);
    auto connect_result = conn->connect();
    ASSERT_TRUE(connect_result) << "Failed to connect: " << connect_result.error().message;

    drop_tables();
    for (const std::string_view ddl :
         {relx::create_table_sql<Account>().to_sql(), relx::create_table_sql<KeyValue>().to_sql(),
          relx::create_table_sql<GridCell>().to_sql(), relx::create_table_sql<Edge>().to_sql()}) {
      auto result = conn->execute_raw(std::string(ddl));
      ASSERT_TRUE(result) << "DDL failed: " << result.error().message;
    }
  }

  void TearDown() override {
    if (conn && conn->is_connected()) {
      drop_tables();
      auto disconnect_result = conn->disconnect();
      EXPECT_TRUE(disconnect_result) << disconnect_result.error().message;
    }
  }

  void drop_tables() {
    for (const char* table : {"sw_accounts", "sw_kv", "sw_grid", "sw_edges"}) {
      auto result = conn->execute_raw(std::string("DROP TABLE IF EXISTS ") + table + " CASCADE;");
      ASSERT_TRUE(result) << result.error().message;
    }
  }

  Account make_account(std::string email, std::string name) {
    return Account{.id = 0,
                   .email = std::move(email),
                   .name = std::move(name),
                   .active = true,
                   .tier = Tier::basic,
                   .note = std::nullopt};
  }
};

}  // namespace

TEST_F(StructWritesIntegrationTest, ValuesFromInsertsAndDatabaseAssignsIdentity) {
  auto insert = relx::insert_into(accounts)
                    .values_from(make_account("ada@example.com", "Ada"))
                    .returning(accounts.id);
  auto rows = conn->fetch_all(insert);
  ASSERT_TRUE(rows) << rows.error().message;
  ASSERT_EQ(rows->size(), 1);
  EXPECT_GT(rows->front().id, 0);

  auto fetched = conn->execute<Account>(
      relx::query::select_all(accounts).where(accounts.email == "ada@example.com"));
  ASSERT_TRUE(fetched) << fetched.error().message;
  EXPECT_EQ(fetched->name, "Ada");
  EXPECT_TRUE(fetched->active);
  EXPECT_EQ(fetched->tier, Tier::basic);
  EXPECT_FALSE(fetched->note.has_value());
}

TEST_F(StructWritesIntegrationTest, ValuesFromMultiRowInsert) {
  auto a = make_account("a@example.com", "A");
  auto b = make_account("b@example.com", "B");
  b.tier = Tier::premium;
  b.note = "vip";

  auto result = conn->execute(relx::insert_into(accounts).values_from(a, b));
  ASSERT_TRUE(result) << result.error().message;

  auto all = conn->execute_many<Account>(
      relx::query::select_all(accounts).order_by(relx::query::asc(accounts.email)));
  ASSERT_TRUE(all) << all.error().message;
  ASSERT_EQ(all->size(), 2);
  EXPECT_EQ((*all)[0].email, "a@example.com");
  EXPECT_EQ((*all)[1].tier, Tier::premium);
  EXPECT_EQ((*all)[1].note, "vip");
}

TEST_F(StructWritesIntegrationTest, UpsertInsertsThenUpdatesOnConflict) {
  const KeyValue original{.key = "theme", .value = "light"};
  auto first = conn->execute(relx::insert_into(kv).values_from(original).upsert());
  ASSERT_TRUE(first) << first.error().message;

  const KeyValue replacement{.key = "theme", .value = "dark"};
  auto second = conn->execute(relx::insert_into(kv).values_from(replacement).upsert());
  ASSERT_TRUE(second) << second.error().message;

  auto fetched = conn->execute<KeyValue>(relx::query::select_all(kv).where(kv.key == "theme"));
  ASSERT_TRUE(fetched) << fetched.error().message;
  EXPECT_EQ(fetched->value, "dark");

  auto count = conn->fetch_one(
      relx::query::select_expr(relx::as<"n", long>(relx::query::count_all())).from(kv));
  ASSERT_TRUE(count) << count.error().message;
  EXPECT_EQ(count->n, 1);
}

TEST_F(StructWritesIntegrationTest, UpsertOnCompositePk) {
  const GridCell cell{.row = 1, .col = 2, .content = "x"};
  ASSERT_TRUE(conn->execute(relx::insert_into(grid).values_from(cell).upsert()));

  const GridCell overwrite{.row = 1, .col = 2, .content = "y"};
  ASSERT_TRUE(conn->execute(relx::insert_into(grid).values_from(overwrite).upsert()));

  const GridCell other{.row = 2, .col = 2, .content = "z"};
  ASSERT_TRUE(conn->execute(relx::insert_into(grid).values_from(other).upsert()));

  auto all = conn->execute_many<GridCell>(
      relx::query::select_all(grid).order_by(relx::query::asc(grid.row)));
  ASSERT_TRUE(all) << all.error().message;
  ASSERT_EQ(all->size(), 2);
  EXPECT_EQ((*all)[0].content, "y");
  EXPECT_EQ((*all)[1].content, "z");
}

TEST_F(StructWritesIntegrationTest, UpsertAllKeyColumnsDoesNothingOnConflict) {
  const Edge e{.src = 1, .dst = 2};
  ASSERT_TRUE(conn->execute(relx::insert_into(edges).values_from(e).upsert()));
  // Second insert of the same key: DO NOTHING, no duplicate-key error
  ASSERT_TRUE(conn->execute(relx::insert_into(edges).values_from(e).upsert()));

  auto all = conn->execute_many<Edge>(relx::query::select_all(edges));
  ASSERT_TRUE(all) << all.error().message;
  EXPECT_EQ(all->size(), 1);
}

TEST_F(StructWritesIntegrationTest, UpsertReturningReflectsFinalRow) {
  const KeyValue original{.key = "lang", .value = "en"};
  ASSERT_TRUE(conn->execute(relx::insert_into(kv).values_from(original).upsert()));

  const KeyValue replacement{.key = "lang", .value = "fr"};
  auto rows = conn->fetch_all(
      relx::insert_into(kv).values_from(replacement).upsert().returning(kv.value));
  ASSERT_TRUE(rows) << rows.error().message;
  ASSERT_EQ(rows->size(), 1);
  EXPECT_EQ(rows->front().value, "fr");
}

TEST_F(StructWritesIntegrationTest, SetFromUpdatesOnlyEngagedFields) {
  ASSERT_TRUE(
      conn->execute(relx::insert_into(accounts).values_from(make_account("p@example.com", "P"))));

  const AccountPatch patch{.name = "Q", .active = std::nullopt, .note = std::nullopt};
  auto update = relx::update(accounts).set_from(patch).where(accounts.email == "p@example.com");
  auto result = conn->execute(update);
  ASSERT_TRUE(result) << result.error().message;

  auto fetched = conn->execute<Account>(
      relx::query::select_all(accounts).where(accounts.email == "p@example.com"));
  ASSERT_TRUE(fetched) << fetched.error().message;
  EXPECT_EQ(fetched->name, "Q");
  EXPECT_TRUE(fetched->active);  // untouched by the patch
  EXPECT_FALSE(fetched->note.has_value());
}

TEST_F(StructWritesIntegrationTest, SetFromCanEngageOptionalColumn) {
  ASSERT_TRUE(
      conn->execute(relx::insert_into(accounts).values_from(make_account("n@example.com", "N"))));

  const AccountPatch patch{.name = std::nullopt, .active = false, .note = "flagged"};
  ASSERT_TRUE(conn->execute(
      relx::update(accounts).set_from(patch).where(accounts.email == "n@example.com")));

  auto fetched = conn->execute<Account>(
      relx::query::select_all(accounts).where(accounts.email == "n@example.com"));
  ASSERT_TRUE(fetched) << fetched.error().message;
  EXPECT_EQ(fetched->name, "N");
  EXPECT_FALSE(fetched->active);
  EXPECT_EQ(fetched->note, "flagged");
}

TEST_F(StructWritesIntegrationTest, SetFromEmptyPatchIsRejectedByServer) {
  ASSERT_TRUE(
      conn->execute(relx::insert_into(accounts).values_from(make_account("e@example.com", "E"))));

  const AccountPatch patch{};
  ASSERT_FALSE(relx::has_engaged_fields(patch));
  auto result = conn->execute(
      relx::update(accounts).set_from(patch).where(accounts.email == "e@example.com"));
  EXPECT_FALSE(result);  // documented behavior: invalid SQL, honest server error
}
