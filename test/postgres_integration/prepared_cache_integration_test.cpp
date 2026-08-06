#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Prepared-statement caching: opt-in per connection, keyed on query type

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("pc_items")]] PcItem {
  [[=relx::ann::pk, =relx::ann::identity]] int id;
  std::string name;
  int quantity;
  std::optional<std::string> note;
};
inline constexpr auto items = relx::t<PcItem>;

// clang-format on

class PreparedCacheIntegrationTest : public ::testing::Test {
protected:
  std::string conn_string =
      "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
  std::unique_ptr<relx::PostgreSQLConnection> conn;

  void SetUp() override {
    conn = std::make_unique<relx::PostgreSQLConnection>(conn_string);
    auto connect_result = conn->connect();
    ASSERT_TRUE(connect_result) << "Failed to connect: " << connect_result.error().message;

    ASSERT_TRUE(conn->execute_raw("DROP TABLE IF EXISTS pc_items CASCADE;"));
    ASSERT_TRUE(conn->execute_raw(std::string(relx::create_table_sql<PcItem>().to_sql())));

    for (int i = 1; i <= 5; ++i) {
      const PcItem item{
          .id = 0, .name = "item" + std::to_string(i), .quantity = i * 10, .note = std::nullopt};
      ASSERT_TRUE(conn->execute(relx::insert_into(items).values_from(item)));
    }
  }

  void TearDown() override {
    if (conn && conn->is_connected()) {
      ASSERT_TRUE(conn->execute_raw("DROP TABLE IF EXISTS pc_items CASCADE;"));
      auto disconnect_result = conn->disconnect();
      EXPECT_TRUE(disconnect_result) << disconnect_result.error().message;
    }
  }
};

}  // namespace

TEST_F(PreparedCacheIntegrationTest, DisabledByDefault) {
  auto rows = conn->fetch_all(
      relx::query::select(items.id, items.name).from(items).where(items.quantity > 20));
  ASSERT_TRUE(rows) << rows.error().message;
  EXPECT_EQ(conn->cached_statement_count(), 0);
}

TEST_F(PreparedCacheIntegrationTest, SameQueryTypePreparesOnce) {
  conn->enable_statement_cache();

  for (const int threshold : {10, 20, 30, 40}) {
    auto rows = conn->fetch_all(relx::query::select(items.id, items.name)
                                    .from(items)
                                    .where(items.quantity > threshold)
                                    .order_by(relx::query::asc(items.id)));
    ASSERT_TRUE(rows) << rows.error().message;
    EXPECT_EQ(rows->size(), static_cast<std::size_t>(5 - threshold / 10));
  }
  EXPECT_EQ(conn->cached_statement_count(), 1);
}

TEST_F(PreparedCacheIntegrationTest, DistinctQueryTypesGetDistinctStatements) {
  conn->enable_statement_cache();

  ASSERT_TRUE(
      conn->fetch_all(relx::query::select(items.id).from(items).where(items.quantity > 10)));
  ASSERT_TRUE(
      conn->fetch_all(relx::query::select(items.name).from(items).where(items.name == "item1")));
  EXPECT_EQ(conn->cached_statement_count(), 2);
}

TEST_F(PreparedCacheIntegrationTest, CachedWritesWork) {
  conn->enable_statement_cache();

  for (int i = 0; i < 3; ++i) {
    auto result = conn->execute(
        relx::update(items).set(items.note, "updated").where(items.quantity == (i + 1) * 10));
    ASSERT_TRUE(result) << result.error().message;
  }
  EXPECT_EQ(conn->cached_statement_count(), 1);

  auto updated = conn->execute_many<PcItem>(
      relx::query::select_all(items).where(items.note == "updated"));
  ASSERT_TRUE(updated) << updated.error().message;
  EXPECT_EQ(updated->size(), 3);
}

TEST_F(PreparedCacheIntegrationTest, TypedNullParamsWork) {
  conn->enable_statement_cache();

  // Same UPDATE type executed with an engaged and a disengaged optional: the SQL text
  // and statement stay identical, only the bound parameter flips to NULL
  auto engage = conn->execute(
      relx::update(items).set(items.note, std::optional<std::string>("x")).where(items.id == 1));
  ASSERT_TRUE(engage) << engage.error().message;
  auto disengage = conn->execute(
      relx::update(items).set(items.note, std::optional<std::string>{}).where(items.id == 1));
  ASSERT_TRUE(disengage) << disengage.error().message;
  EXPECT_EQ(conn->cached_statement_count(), 1);

  auto fetched = conn->execute<PcItem>(relx::query::select_all(items).where(items.id == 1));
  ASSERT_TRUE(fetched) << fetched.error().message;
  EXPECT_FALSE(fetched->note.has_value());
}

TEST_F(PreparedCacheIntegrationTest, DisconnectClearsCache) {
  conn->enable_statement_cache();
  ASSERT_TRUE(
      conn->fetch_all(relx::query::select(items.id).from(items).where(items.quantity > 10)));
  EXPECT_EQ(conn->cached_statement_count(), 1);

  ASSERT_TRUE(conn->disconnect());
  EXPECT_EQ(conn->cached_statement_count(), 0);

  ASSERT_TRUE(conn->connect());
  conn->enable_statement_cache();
  auto rows = conn->fetch_all(relx::query::select(items.id).from(items).where(items.quantity > 10));
  ASSERT_TRUE(rows) << rows.error().message;
  EXPECT_EQ(conn->cached_statement_count(), 1);
}

TEST_F(PreparedCacheIntegrationTest, SurvivesUnrelatedSchemaChange) {
  conn->enable_statement_cache();
  ASSERT_TRUE(conn->fetch_all(
      relx::query::select(items.id, items.name).from(items).where(items.quantity > 10)));

  // Adding an unrelated column must not break the cached statement (the server
  // revalidates prepared plans against catalog changes)
  ASSERT_TRUE(conn->execute_raw("ALTER TABLE pc_items ADD COLUMN extra int;"));

  auto rows = conn->fetch_all(
      relx::query::select(items.id, items.name).from(items).where(items.quantity > 10));
  ASSERT_TRUE(rows) << rows.error().message;
  EXPECT_EQ(rows->size(), 4);
}

TEST_F(PreparedCacheIntegrationTest, RuntimeShapedQueriesBypassCache) {
  conn->enable_statement_cache();

  // Dynamic IN-lists are not static-shaped; they must execute unprepared
  auto rows = conn->execute_many<PcItem>(relx::query::select_all(items).where(relx::query::in(
      relx::query::column_ref(items.name), std::vector<std::string>{"item1", "item2"})));
  ASSERT_TRUE(rows) << rows.error().message;
  EXPECT_EQ(rows->size(), 2);
  EXPECT_EQ(conn->cached_statement_count(), 0);
}
