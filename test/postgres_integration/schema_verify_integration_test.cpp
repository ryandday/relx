#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// verify_schema against a real PostgreSQL: drift detection between the reflected
// table definitions and information_schema

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

enum class SvStatus { active, retired };

struct [[=relx::table("sv_devices")]] Device {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::unique]] std::string serial;
  std::string name;
  bool online;
  double battery;
  std::optional<std::string> location;
};
inline constexpr auto devices = relx::t<Device>;

struct [[=relx::table("sv_readings"), =relx::ann::composite_pk("device_id", "taken_at")]]
Reading {
  int device_id;
  std::string taken_at;
  double value;
};

// Same table name as Device but a drifted definition (extra field, changed types)
struct [[=relx::table("sv_devices")]] DriftedDevice {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::unique]] std::string serial;
  std::string name;
  bool online;
  std::string battery;             // type drift: double -> string
  std::optional<std::string> location;
  int firmware;                    // missing in DB
};

// clang-format on

class SchemaVerifyIntegrationTest : public ::testing::Test {
protected:
  std::string conn_string =
      "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
  std::unique_ptr<relx::PostgreSQLConnection> conn;

  void SetUp() override {
    conn = std::make_unique<relx::PostgreSQLConnection>(conn_string);
    auto connect_result = conn->connect();
    ASSERT_TRUE(connect_result) << "Failed to connect: " << connect_result.error().message;

    drop_tables();
    ASSERT_TRUE(conn->execute_raw(std::string(relx::create_table_sql<Device>().to_sql())));
    ASSERT_TRUE(conn->execute_raw(std::string(relx::create_table_sql<Reading>().to_sql())));
  }

  void TearDown() override {
    if (conn && conn->is_connected()) {
      drop_tables();
      auto disconnect_result = conn->disconnect();
      EXPECT_TRUE(disconnect_result) << disconnect_result.error().message;
    }
  }

  void drop_tables() {
    ASSERT_TRUE(conn->execute_raw("DROP TABLE IF EXISTS sv_readings CASCADE;"));
    ASSERT_TRUE(conn->execute_raw("DROP TABLE IF EXISTS sv_devices CASCADE;"));
  }
};

}  // namespace

TEST_F(SchemaVerifyIntegrationTest, MatchingSchemaVerifiesClean) {
  auto verified = relx::verify_schema<Device, Reading>(*conn);
  EXPECT_TRUE(verified) << verified.error().message();
}

TEST_F(SchemaVerifyIntegrationTest, MissingTableIsReported) {
  ASSERT_TRUE(conn->execute_raw("DROP TABLE sv_readings;"));

  auto verified = relx::verify_schema<Device, Reading>(*conn);
  ASSERT_FALSE(verified);
  const auto& error = verified.error();
  ASSERT_FALSE(error.connection_error.has_value());
  ASSERT_EQ(error.drift.size(), 1);
  EXPECT_EQ(error.drift.front().table, "sv_readings");
  EXPECT_TRUE(error.drift.front().missing_table);
  EXPECT_NE(error.message().find("does not exist"), std::string::npos);
}

TEST_F(SchemaVerifyIntegrationTest, TypeAndMissingColumnDriftIsReported) {
  auto verified = relx::verify_schema<DriftedDevice>(*conn);
  ASSERT_FALSE(verified);
  const auto& error = verified.error();
  ASSERT_EQ(error.drift.size(), 1);
  const auto& table = error.drift.front();
  EXPECT_EQ(table.table, "sv_devices");
  EXPECT_FALSE(table.missing_table);

  bool saw_type_mismatch = false;
  bool saw_missing = false;
  for (const auto& column : table.columns) {
    if (column.column == "battery" && column.kind == relx::ColumnDrift::Kind::TypeMismatch) {
      saw_type_mismatch = true;
      EXPECT_EQ(column.expected, "text");
      EXPECT_EQ(column.actual, "double precision");
    }
    if (column.column == "firmware" && column.kind == relx::ColumnDrift::Kind::MissingColumn) {
      saw_missing = true;
    }
  }
  EXPECT_TRUE(saw_type_mismatch) << error.message();
  EXPECT_TRUE(saw_missing) << error.message();
}

TEST_F(SchemaVerifyIntegrationTest, ExtraDatabaseColumnIsReported) {
  ASSERT_TRUE(conn->execute_raw("ALTER TABLE sv_devices ADD COLUMN legacy_flag boolean;"));

  auto verified = relx::verify_schema<Device>(*conn);
  ASSERT_FALSE(verified);
  const auto& table = verified.error().drift.front();
  ASSERT_EQ(table.columns.size(), 1);
  EXPECT_EQ(table.columns.front().kind, relx::ColumnDrift::Kind::ExtraColumn);
  EXPECT_EQ(table.columns.front().column, "legacy_flag");
}

TEST_F(SchemaVerifyIntegrationTest, NullabilityDriftIsReported) {
  ASSERT_TRUE(conn->execute_raw("ALTER TABLE sv_devices ALTER COLUMN name DROP NOT NULL;"));

  auto verified = relx::verify_schema<Device>(*conn);
  ASSERT_FALSE(verified);
  const auto& table = verified.error().drift.front();
  ASSERT_EQ(table.columns.size(), 1);
  EXPECT_EQ(table.columns.front().kind, relx::ColumnDrift::Kind::NullabilityMismatch);
  EXPECT_EQ(table.columns.front().column, "name");
  EXPECT_EQ(table.columns.front().expected, "NOT NULL");
  EXPECT_EQ(table.columns.front().actual, "nullable");
}

TEST_F(SchemaVerifyIntegrationTest, PrimaryKeyDriftIsReported) {
  ASSERT_TRUE(conn->execute_raw("ALTER TABLE sv_readings DROP CONSTRAINT sv_readings_pkey;"));
  ASSERT_TRUE(conn->execute_raw("ALTER TABLE sv_readings ADD PRIMARY KEY (device_id);"));

  auto verified = relx::verify_schema<Reading>(*conn);
  ASSERT_FALSE(verified);
  const auto& table = verified.error().drift.front();
  EXPECT_TRUE(table.pk_mismatch);
  ASSERT_EQ(table.pk_expected.size(), 2);
  ASSERT_EQ(table.pk_actual.size(), 1);
  EXPECT_EQ(table.pk_actual.front(), "device_id");
}

TEST_F(SchemaVerifyIntegrationTest, CompositePkOrderMatchesDeclaration) {
  // The composite pk is (device_id, taken_at) in declaration order - a clean schema
  // must verify clean including key order
  auto verified = relx::verify_schema<Reading>(*conn);
  EXPECT_TRUE(verified) << verified.error().message();
}
