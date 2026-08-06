#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Nested row synthesis against a real PostgreSQL: whole-table selects over joins

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("nri_authors")]] Author {
  [[=relx::ann::pk]] int id;
  std::string name;
};
inline constexpr auto authors = relx::t<Author>;

struct [[=relx::table("nri_books")]] Book {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Author::id>]] int author_id;
  std::string title;
  std::optional<std::string> subtitle;
};
inline constexpr auto books = relx::t<Book>;

// clang-format on

class NestedRowIntegrationTest : public ::testing::Test {
protected:
  std::string conn_string =
      "host=localhost port=5434 dbname=relx_test user=postgres password=postgres";
  std::unique_ptr<relx::PostgreSQLConnection> conn;

  void SetUp() override {
    conn = std::make_unique<relx::PostgreSQLConnection>(conn_string);
    auto connect_result = conn->connect();
    ASSERT_TRUE(connect_result) << "Failed to connect: " << connect_result.error().message;

    drop_tables();
    ASSERT_TRUE(conn->execute_raw(std::string(relx::create_table_sql<Author>().to_sql())));
    ASSERT_TRUE(conn->execute_raw(std::string(relx::create_table_sql<Book>().to_sql())));

    // Two authors; Ada has two books, Bob has none
    const Author ada{.id = 1, .name = "Ada"};
    const Author bob{.id = 2, .name = "Bob"};
    ASSERT_TRUE(conn->execute(relx::insert_into(authors).values_from(ada, bob)));

    const Book b1{.id = 10, .author_id = 1, .title = "Notes", .subtitle = std::nullopt};
    const Book b2{.id = 11, .author_id = 1, .title = "Engines", .subtitle = "analytical"};
    ASSERT_TRUE(conn->execute(relx::insert_into(books).values_from(b1, b2)));
  }

  void TearDown() override {
    if (conn && conn->is_connected()) {
      drop_tables();
      auto disconnect_result = conn->disconnect();
      EXPECT_TRUE(disconnect_result) << disconnect_result.error().message;
    }
  }

  void drop_tables() {
    ASSERT_TRUE(conn->execute_raw("DROP TABLE IF EXISTS nri_books CASCADE;"));
    ASSERT_TRUE(conn->execute_raw("DROP TABLE IF EXISTS nri_authors CASCADE;"));
  }
};

}  // namespace

TEST_F(NestedRowIntegrationTest, InnerJoinNestsBothTables) {
  auto query = relx::select(authors, books)
                   .from(authors)
                   .join(books, relx::on(authors.id == books.author_id))
                   .order_by(relx::query::asc(books.id));
  auto rows = conn->fetch_all(query);
  ASSERT_TRUE(rows) << rows.error().message;
  ASSERT_EQ(rows->size(), 2);

  const auto& first = rows->front();
  EXPECT_EQ(first.nri_authors.id, 1);
  EXPECT_EQ(first.nri_authors.name, "Ada");
  EXPECT_EQ(first.nri_books.id, 10);
  EXPECT_EQ(first.nri_books.title, "Notes");
  EXPECT_FALSE(first.nri_books.subtitle.has_value());
  EXPECT_EQ(rows->back().nri_books.subtitle, "analytical");
}

TEST_F(NestedRowIntegrationTest, LeftJoinNonMatchingSideIsNullopt) {
  auto query = relx::select(authors, books)
                   .from(authors)
                   .left_join(books, relx::on(authors.id == books.author_id))
                   .order_by(relx::query::asc(authors.id));
  auto rows = conn->fetch_all(query);
  ASSERT_TRUE(rows) << rows.error().message;
  ASSERT_EQ(rows->size(), 3);  // Ada x2 books + Bob x1 null row

  // Bob's row: no matching book
  const auto& bob_row = rows->back();
  EXPECT_EQ(bob_row.nri_authors.name, "Bob");
  EXPECT_FALSE(bob_row.nri_books.has_value());

  // Ada's rows carry real books
  EXPECT_TRUE(rows->front().nri_books.has_value());
  EXPECT_EQ(rows->front().nri_books->author_id, 1);
}

TEST_F(NestedRowIntegrationTest, MixedTableAndScalarSelect) {
  auto query = relx::select(books, relx::as<"author_name">(authors.name))
                   .from(books)
                   .join(authors, relx::on(books.author_id == authors.id))
                   .where(books.id == 11);
  auto row = conn->fetch_one(query);
  ASSERT_TRUE(row) << row.error().message;
  EXPECT_EQ(row->nri_books.title, "Engines");
  EXPECT_EQ(row->author_name, "Ada");
}

TEST_F(NestedRowIntegrationTest, SingleWholeTableSelectRoundTrips) {
  auto query = relx::select(authors).from(authors).order_by(relx::query::asc(authors.id));
  auto rows = conn->fetch_all(query);
  ASSERT_TRUE(rows) << rows.error().message;
  ASSERT_EQ(rows->size(), 2);
  EXPECT_EQ(rows->front().nri_authors.name, "Ada");
  EXPECT_EQ(rows->back().nri_authors.name, "Bob");
}

TEST_F(NestedRowIntegrationTest, DmlReturningFetchesReturningList) {
  const Author carol{.id = 3, .name = "Carol"};
  auto rows = conn->fetch_all(relx::insert_into(authors).values_from(carol).returning(authors.id));
  ASSERT_TRUE(rows) << rows.error().message;
  ASSERT_EQ(rows->size(), 1);
  EXPECT_EQ(rows->front().id, 3);

  auto updated = conn->fetch_all(relx::update(authors)
                                     .set(authors.name, "Caroline")
                                     .where(authors.id == 3)
                                     .returning(authors.id, authors.name));
  ASSERT_TRUE(updated) << updated.error().message;
  ASSERT_EQ(updated->size(), 1);
  EXPECT_EQ(updated->front().name, "Caroline");
}
