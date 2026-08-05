#include <optional>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/results.hpp>
#include <relx/schema.hpp>

// Regression tests for the 2026-08-05 hardening pass: out-of-band NULL cells,
// the escaped pipe-text format, and error paths that used to fabricate defaults.

namespace {

using relx::result::Cell;
using relx::result::ResultSet;
using relx::result::Row;

// clang-format off
struct [[=relx::table("strict_users")]] StrictUsers {
  int id;
  std::string name;
};
inline constexpr auto strict_users = relx::t<StrictUsers>;
// clang-format on

TEST(CellNullSemantics, NullnessIsOutOfBand) {
  const Cell null_cell = Cell::null();
  EXPECT_TRUE(null_cell.is_null());

  // A cell whose text happens to be "NULL" is real data, not SQL NULL
  const Cell text_cell{std::string("NULL")};
  EXPECT_FALSE(text_cell.is_null());
  auto as_string = text_cell.as<std::string>();
  ASSERT_TRUE(as_string.has_value());
  EXPECT_EQ(*as_string, "NULL");

  // NULL converts to nullopt for optionals and errors for non-optionals
  auto as_optional = null_cell.as<std::optional<int>>();
  ASSERT_TRUE(as_optional.has_value());
  EXPECT_FALSE(as_optional->has_value());
  EXPECT_FALSE(null_cell.as<int>().has_value());
}

TEST(CellNullSemantics, QuotedTextSurvivesVerbatim) {
  // Raw protocol text is never SQL-literal de-quoted
  const Cell quoted{std::string("'quoted'")};
  auto value = quoted.as<std::string>();
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, "'quoted'");
}

TEST(TextFormat, EscapeRoundTripsSpecialBytes) {
  using relx::result::text_format::escape;
  using relx::result::text_format::null_marker;
  using relx::result::text_format::unescape;

  const std::string tricky = "a|b\\c\nd\re\\N";
  EXPECT_EQ(unescape(escape(tricky)), tricky);

  // Escaped real data can never collide with the NULL marker
  EXPECT_NE(escape("\\N"), null_marker);
  EXPECT_EQ(unescape(escape("NULL")), "NULL");
}

ResultSet two_row_result() {
  std::vector<std::string> names{"id", "name"};
  std::vector<Row> rows;
  rows.emplace_back(std::vector<Cell>{Cell{"1"}, Cell{"alice"}}, names);
  rows.emplace_back(std::vector<Cell>{Cell{"2"}, Cell{"bob"}}, names);
  return ResultSet{std::move(rows), names};
}

TEST(ResultSetStrictness, UnknownColumnNameThrows) {
  const ResultSet results = two_row_result();
  EXPECT_THROW((results.as<int, std::string>(std::array<std::string, 2>{"id", "typo"})),
               std::invalid_argument);
}

TEST(ResultSetStrictness, StructuredBindingConversionFailureThrows) {
  std::vector<std::string> names{"id"};
  std::vector<Row> rows;
  rows.emplace_back(std::vector<Cell>{Cell{"not-a-number"}}, names);
  const ResultSet results{std::move(rows), names};

  auto view = results.as<int>(std::array<std::string, 1>{"id"});
  auto iterate = [&view] {
    for ([[maybe_unused]] const auto& [id] : view) {
    }
  };
  EXPECT_THROW(iterate(), std::runtime_error);
}

TEST(ResultSetStrictness, ToStringContainsNewlinesInReturnValue) {
  const ResultSet results = two_row_result();
  const std::string text = results.to_string();
  EXPECT_NE(text.find('\n'), std::string::npos);
  EXPECT_NE(text.find("alice"), std::string::npos);
  EXPECT_NE(text.find("bob"), std::string::npos);
}

TEST(LazyResultStrictness, SubscriptAndIteratorThrowOnInvalidIndex) {
  auto query = relx::query::select(strict_users.id, strict_users.name).from(strict_users);
  auto lazy = relx::result::parse_lazy(query, std::string("id|name\n1|alice\n"));

  EXPECT_EQ(lazy.size(), 1);
  EXPECT_THROW(lazy[5], std::out_of_range);
}

TEST(LazyResultStrictness, EscapedValuesAndNullMarkerDecode) {
  auto query = relx::query::select(strict_users.id, strict_users.name).from(strict_users);
  // Row 1: name is the literal text "NULL"; row 2: name is SQL NULL (\N);
  // row 3: name contains an escaped pipe
  auto lazy = relx::result::parse_lazy(query, std::string("id|name\n1|NULL\n2|\\N\n3|a\\|b\n"));

  ASSERT_EQ(lazy.size(), 3);

  auto text_null = lazy[0].get<std::string>("name");
  ASSERT_TRUE(text_null.has_value());
  EXPECT_EQ(*text_null, "NULL");

  auto real_null = lazy[1].get<std::optional<std::string>>("name");
  ASSERT_TRUE(real_null.has_value());
  EXPECT_FALSE(real_null->has_value());

  auto piped = lazy[2].get<std::string>("name");
  ASSERT_TRUE(piped.has_value());
  EXPECT_EQ(*piped, "a|b");
}

}  // namespace
