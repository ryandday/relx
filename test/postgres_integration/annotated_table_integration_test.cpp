#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

// One struct is both the schema definition and the result DTO
struct [[=relx::table("annotated_products")]] Product {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::unique]] std::string sku;
  std::string name;
  double price;
  [[=relx::default_value<true>{}]] bool in_stock;
  std::optional<std::string> notes;
};
inline constexpr auto products = relx::t<Product>;

class AnnotatedTableIntegrationTest : public ::testing::Test {
protected:
  std::string conn_string =
      "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
  std::unique_ptr<relx::PostgreSQLConnection> conn;

  void SetUp() override {
    conn = std::make_unique<relx::PostgreSQLConnection>(conn_string);
    auto connect_result = conn->connect();
    ASSERT_TRUE(connect_result) << "Failed to connect: " << connect_result.error().message;

    auto drop_result = conn->execute_raw("DROP TABLE IF EXISTS annotated_products;");
    ASSERT_TRUE(drop_result) << drop_result.error().message;

    auto create_result = conn->execute(relx::create_table(products).if_not_exists());
    ASSERT_TRUE(create_result) << "Failed to create table: " << create_result.error().message;
  }

  void TearDown() override {
    if (conn && conn->is_connected()) {
      auto drop_result = conn->execute_raw("DROP TABLE IF EXISTS annotated_products;");
      EXPECT_TRUE(drop_result) << drop_result.error().message;
      auto disconnect_result = conn->disconnect();
      EXPECT_TRUE(disconnect_result) << disconnect_result.error().message;
    }
  }
};

TEST_F(AnnotatedTableIntegrationTest, InsertAndSelectRoundTrip) {
  auto insert = relx::query::insert_into(products)
                    .columns(products.id, products.sku, products.name, products.price)
                    .values(1, "SKU-001", "Widget", 9.99)
                    .values(2, "SKU-002", "Gadget", 24.5);
  auto insert_result = conn->execute(insert);
  ASSERT_TRUE(insert_result) << insert_result.error().message;

  auto query = relx::query::select(products.id, products.sku, products.name, products.price,
                                   products.in_stock, products.notes)
                   .from(products)
                   .order_by(products.id);

  // The annotated schema struct is itself the DTO
  auto result = conn->execute_many<Product>(query);
  ASSERT_TRUE(result) << result.error().message;

  const auto& rows = *result;
  ASSERT_EQ(rows.size(), 2);

  EXPECT_EQ(rows[0].id, 1);
  EXPECT_EQ(rows[0].sku, "SKU-001");
  EXPECT_EQ(rows[0].name, "Widget");
  EXPECT_DOUBLE_EQ(rows[0].price, 9.99);
  EXPECT_TRUE(rows[0].in_stock);           // default applied by the database
  EXPECT_FALSE(rows[0].notes.has_value());  // NULL -> nullopt

  EXPECT_EQ(rows[1].id, 2);
  EXPECT_EQ(rows[1].sku, "SKU-002");
}

TEST_F(AnnotatedTableIntegrationTest, WhereAndUpdate) {
  auto insert = relx::query::insert_into(products)
                    .columns(products.id, products.sku, products.name, products.price)
                    .values(7, "SKU-007", "Sprocket", 3.5);
  ASSERT_TRUE(conn->execute(insert));

  auto update = relx::query::update(products)
                    .set(products.price, 4.25)
                    .where(products.id == 7);
  ASSERT_TRUE(conn->execute(update));

  auto query = relx::query::select(products.id, products.sku, products.name, products.price,
                                   products.in_stock, products.notes)
                   .from(products)
                   .where(products.sku == "SKU-007");
  auto result = conn->execute<Product>(query);
  ASSERT_TRUE(result) << result.error().message;
  EXPECT_DOUBLE_EQ(result->price, 4.25);
}

TEST_F(AnnotatedTableIntegrationTest, FetchWithSynthesizedRows) {
  auto insert = relx::query::insert_into(products)
                    .columns(products.id, products.sku, products.name, products.price)
                    .values(1, "SKU-A", "Widget", 10.0)
                    .values(2, "SKU-B", "Widget", 14.0)
                    .values(3, "SKU-C", "Gadget", 30.0);
  ASSERT_TRUE(conn->execute(insert));

  // No hand-written DTO: the row type is synthesized from the select list
  auto q = relx::query::select(products.id, products.sku, products.notes)
               .from(products)
               .order_by(products.id);
  auto rows = conn->fetch_all(q);
  ASSERT_TRUE(rows) << rows.error().message;
  ASSERT_EQ(rows->size(), 3);
  EXPECT_EQ((*rows)[0].id, 1);
  EXPECT_EQ((*rows)[2].sku, "SKU-C");
  EXPECT_FALSE((*rows)[0].notes.has_value());

  // Aggregates via compile-time alias with explicit result type
  auto agg = relx::query::select(products.name,
                                 relx::as<"n", long>(relx::count(products.id)),
                                 relx::as<"total", double>(relx::sum(products.price)))
                 .from(products)
                 .group_by(products.name)
                 .order_by(products.name);
  auto agg_rows = conn->fetch_all(agg);
  ASSERT_TRUE(agg_rows) << agg_rows.error().message;
  ASSERT_EQ(agg_rows->size(), 2);
  EXPECT_EQ((*agg_rows)[0].name, "Gadget");
  EXPECT_EQ((*agg_rows)[0].n, 1);
  EXPECT_DOUBLE_EQ((*agg_rows)[0].total, 30.0);
  EXPECT_EQ((*agg_rows)[1].name, "Widget");
  EXPECT_EQ((*agg_rows)[1].n, 2);
  EXPECT_DOUBLE_EQ((*agg_rows)[1].total, 24.0);

  auto one = conn->fetch_one(
      relx::query::select(products.sku).from(products).where(products.id == 2));
  ASSERT_TRUE(one) << one.error().message;
  EXPECT_EQ(one->sku, "SKU-B");
}

// clang-format on

}  // namespace
