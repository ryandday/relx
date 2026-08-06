#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/json.hpp>
#include <relx/query.hpp>
#include <relx/query/example_filter.hpp>
#include <relx/refl_types.hpp>
#include <relx/schema.hpp>

// Struct-typed JSONB columns + utility types against a real PostgreSQL

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::ann::jsonb]] Geo {
  double lat;
  double lon;
};

struct [[=relx::ann::jsonb]] Meta {
  std::string device;
  int version;
  std::optional<std::string> note;
  std::vector<int> tags;
  Geo geo;
};

struct [[=relx::table("ji_events")]] JiEvent {
  [[=relx::ann::pk, =relx::ann::identity]] int id;
  std::string kind;
  bool resolved;
  Meta metadata;
  std::optional<Geo> last_location;
};
inline constexpr auto events = relx::t<JiEvent>;

struct EventPatch {
  std::optional<Meta> metadata;
};

// clang-format on

class JsonbIntegrationTest : public ::testing::Test {
protected:
  std::string conn_string =
      "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
  std::unique_ptr<relx::PostgreSQLConnection> conn;

  void SetUp() override {
    conn = std::make_unique<relx::PostgreSQLConnection>(conn_string);
    auto connect_result = conn->connect();
    ASSERT_TRUE(connect_result) << "Failed to connect: " << connect_result.error().message;

    ASSERT_TRUE(conn->execute_raw("DROP TABLE IF EXISTS ji_events CASCADE;"));
    auto created = conn->execute_raw(std::string(relx::create_table_sql<JiEvent>().to_sql()));
    ASSERT_TRUE(created) << created.error().message;
  }

  void TearDown() override {
    if (conn && conn->is_connected()) {
      ASSERT_TRUE(conn->execute_raw("DROP TABLE IF EXISTS ji_events CASCADE;"));
      auto disconnect_result = conn->disconnect();
      EXPECT_TRUE(disconnect_result) << disconnect_result.error().message;
    }
  }

  JiEvent make_event(std::string kind) {
    return JiEvent{.id = 0,
                   .kind = std::move(kind),
                   .resolved = false,
                   .metadata = {.device = "kiosk-7",
                                .version = 2,
                                .note = std::nullopt,
                                .tags = {3, 5},
                                .geo = {.lat = 52.5, .lon = 13.4}},
                   .last_location = std::nullopt};
  }
};

}  // namespace

TEST_F(JsonbIntegrationTest, JsonbRoundTripsThroughInsertAndSelect) {
  ASSERT_TRUE(conn->execute(relx::insert_into(events).values_from(make_event("boot"))));

  auto fetched = conn->execute<JiEvent>(
      relx::query::select_all(events).where(events.kind == "boot"));
  ASSERT_TRUE(fetched) << fetched.error().message;
  EXPECT_EQ(fetched->metadata.device, "kiosk-7");
  EXPECT_EQ(fetched->metadata.version, 2);
  EXPECT_FALSE(fetched->metadata.note.has_value());
  EXPECT_EQ(fetched->metadata.tags, (std::vector<int>{3, 5}));
  EXPECT_EQ(fetched->metadata.geo.lat, 52.5);
  EXPECT_FALSE(fetched->last_location.has_value());
}

TEST_F(JsonbIntegrationTest, OptionalJsonbColumnRoundTrips) {
  auto event = make_event("move");
  event.last_location = Geo{.lat = 1.5, .lon = -2.25};
  ASSERT_TRUE(conn->execute(relx::insert_into(events).values_from(event)));

  auto fetched = conn->execute<JiEvent>(
      relx::query::select_all(events).where(events.kind == "move"));
  ASSERT_TRUE(fetched) << fetched.error().message;
  ASSERT_TRUE(fetched->last_location.has_value());
  EXPECT_EQ(fetched->last_location->lat, 1.5);
  EXPECT_EQ(fetched->last_location->lon, -2.25);
}

TEST_F(JsonbIntegrationTest, JsonbSurvivesServerSideNormalization) {
  // JSONB normalizes key order and whitespace server-side; strict decode must still
  // accept what comes back
  auto event = make_event("norm");
  event.metadata.note = "n\"ote\n";
  ASSERT_TRUE(conn->execute(relx::insert_into(events).values_from(event)));

  auto fetched = conn->execute<JiEvent>(
      relx::query::select_all(events).where(events.kind == "norm"));
  ASSERT_TRUE(fetched) << fetched.error().message;
  EXPECT_EQ(fetched->metadata.note, "n\"ote\n");
}

TEST_F(JsonbIntegrationTest, JsonbInSetFromPatch) {
  ASSERT_TRUE(conn->execute(relx::insert_into(events).values_from(make_event("patch"))));

  EventPatch patch;
  patch.metadata = Meta{.device = "kiosk-9",
                        .version = 3,
                        .note = "swapped",
                        .tags = {},
                        .geo = {.lat = 0, .lon = 0}};
  ASSERT_TRUE(conn->execute(relx::update(events).set_from(patch).where(events.kind == "patch")));

  auto fetched = conn->execute<JiEvent>(
      relx::query::select_all(events).where(events.kind == "patch"));
  ASSERT_TRUE(fetched) << fetched.error().message;
  EXPECT_EQ(fetched->metadata.device, "kiosk-9");
  EXPECT_EQ(fetched->metadata.note, "swapped");
}

TEST_F(JsonbIntegrationTest, WhereEqualsFiltersByExample) {
  auto resolved_event = make_event("done");
  resolved_event.resolved = true;
  ASSERT_TRUE(
      conn->execute(relx::insert_into(events).values_from(make_event("open"), resolved_event)));

  relx::refl::partial<relx::refl::pick<JiEvent, ^^JiEvent::kind, ^^JiEvent::resolved>> example{};
  example.resolved = true;

  auto rows = conn->execute_many<JiEvent>(
      relx::query::select_all(events).where(relx::query::where_equals(events, example)));
  ASSERT_TRUE(rows) << rows.error().message;
  ASSERT_EQ(rows->size(), 1);
  EXPECT_EQ(rows->front().kind, "done");

  // Empty example matches every row
  const relx::refl::partial<relx::refl::pick<JiEvent, ^^JiEvent::kind>> everything{};
  auto all = conn->execute_many<JiEvent>(
      relx::query::select_all(events).where(relx::query::where_equals(events, everything)));
  ASSERT_TRUE(all) << all.error().message;
  EXPECT_EQ(all->size(), 2);
}
