#include <optional>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Nested row synthesis: select(users, posts) groups columns by table into nested
// row members; a left-joined table becomes std::optional.

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

namespace {

struct [[=relx::table("nr_authors")]] Author {
  [[=relx::ann::pk]] int id;
  std::string name;
};
constexpr auto authors = relx::t<Author>;

struct [[=relx::table("nr_books")]] Book {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Author::id>]] int author_id;
  std::string title;
  std::optional<std::string> subtitle;
};
constexpr auto books = relx::t<Book>;

// Classic-DSL table, to prove nested rows synthesize a value struct for it
struct ClassicTag {
  static constexpr auto table_name = "nr_tags";
  relx::schema::column<ClassicTag, "id", int, relx::schema::primary_key> id;
  relx::schema::column<ClassicTag, "label", std::string> label;
};

}  // namespace

// clang-format on

TEST(NestedRowTest, WholeTableSelectExpandsQualifiedColumnList) {
  auto query = relx::select(authors, books)
                   .from(authors)
                   .join(books, relx::on(authors.id == books.author_id));

  EXPECT_EQ(query.to_sql(),
            "SELECT nr_authors.id, nr_authors.name, nr_books.id, nr_books.author_id, "
            "nr_books.title, nr_books.subtitle FROM nr_authors "
            "JOIN nr_books ON (nr_authors.id = nr_books.author_id)");
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(NestedRowTest, InnerJoinRowNestsBothTablesByValue) {
  auto query = relx::select(authors, books)
                   .from(authors)
                   .join(books, relx::on(authors.id == books.author_id));
  using Row = relx::row_type_for<decltype(query)>;

  static_assert(std::is_same_v<decltype(Row{}.nr_authors), Author>);
  static_assert(std::is_same_v<decltype(Row{}.nr_books), Book>);
  SUCCEED();
}

TEST(NestedRowTest, LeftJoinedTableBecomesOptional) {
  auto query = relx::select(authors, books)
                   .from(authors)
                   .left_join(books, relx::on(authors.id == books.author_id));
  using Row = relx::row_type_for<decltype(query)>;

  static_assert(std::is_same_v<decltype(Row{}.nr_authors), Author>);
  static_assert(std::is_same_v<decltype(Row{}.nr_books), std::optional<Book>>);
  SUCCEED();
}

TEST(NestedRowTest, MixedTableAndAliasedScalar) {
  auto query = relx::select(authors, relx::as<"book_title">(books.title))
                   .from(authors)
                   .join(books, relx::on(authors.id == books.author_id));
  using Row = relx::row_type_for<decltype(query)>;

  static_assert(std::is_same_v<decltype(Row{}.nr_authors), Author>);
  static_assert(std::is_same_v<decltype(Row{}.book_title), std::string>);
  SUCCEED();
}

TEST(NestedRowTest, ClassicTableSynthesizesValueStruct) {
  constexpr ClassicTag tags{};
  auto query = relx::select(tags).from(tags);

  EXPECT_EQ(query.to_sql(), "SELECT nr_tags.id, nr_tags.label FROM nr_tags");

  using Row = relx::row_type_for<decltype(query)>;
  static_assert(std::is_same_v<decltype(Row{}.nr_tags.id), int>);
  static_assert(std::is_same_v<decltype(Row{}.nr_tags.label), std::string>);
  SUCCEED();
}

TEST(NestedRowTest, DmlReturningSynthesizesFromReturningList) {
  auto insert = relx::insert_into(authors)
                    .columns(authors.id, authors.name)
                    .values(1, "a")
                    .returning(authors.id);
  using Row = relx::row_type_for<decltype(insert)>;

  static_assert(relx::refl::field_count<Row>() == 1);
  static_assert(std::is_same_v<decltype(Row{}.id), int>);

  auto update = relx::update(authors)
                    .set(authors.name, "b")
                    .where(authors.id == 1)
                    .returning(authors.id, authors.name);
  using URow = relx::row_type_for<decltype(update)>;
  static_assert(relx::refl::field_count<URow>() == 2);
  static_assert(std::is_same_v<decltype(URow{}.name), std::string>);
  SUCCEED();
}
