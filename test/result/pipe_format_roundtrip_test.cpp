#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/results.hpp>

// Round-trip property tests for the internal pipe-delimited text format shared by the
// streaming writers (src/postgres), result::parse, parse_lazy, and LazyRow. The wire
// contract: cells join with '|' (N separators, N+1 cells - trailing empties survive),
// rows terminate with '\n', NULL is the out-of-band marker \N, and escape() makes any
// byte sequence round-trip.

namespace {

using relx::result::text_format::escape;
using relx::result::text_format::null_marker;
using relx::result::text_format::split_cells;
using relx::result::text_format::unescape;

// parse()/parse_lazy() take the query only for its type; any SqlExpr works
struct FakeQuery {
  std::string to_sql() const { return "SELECT 1"; }
  std::vector<std::string> bind_params() const { return {}; }
};

// Mirrors the row serialization in PostgreSQLStreamingSource::format_row and
// PostgreSQLAsyncStreamingSource::format_single_row: '|'-joined, NULL as \N
std::string write_row(const std::vector<std::optional<std::string>>& cells) {
  std::string out;
  for (size_t i = 0; i < cells.size(); ++i) {
    if (i > 0) {
      out += '|';
    }
    if (!cells[i]) {
      out += null_marker;
    } else {
      out += escape(*cells[i]);
    }
  }
  return out;
}

std::vector<std::optional<std::string>> read_row(std::string_view encoded) {
  std::vector<std::optional<std::string>> cells;
  for (const auto raw : split_cells(encoded)) {
    if (raw == null_marker) {
      cells.emplace_back(std::nullopt);
    } else {
      cells.emplace_back(unescape(raw));
    }
  }
  return cells;
}

TEST(PipeFormatRoundTrip, TrailingEmptyCellSurvives) {
  const std::vector<std::optional<std::string>> cells = {"a", "b", ""};
  EXPECT_EQ(write_row(cells), "a|b|");
  EXPECT_EQ(read_row("a|b|"), cells);
}

TEST(PipeFormatRoundTrip, AllEmptySingleCellRow) {
  const std::vector<std::optional<std::string>> cells = {""};
  EXPECT_EQ(write_row(cells), "");
  EXPECT_EQ(read_row(""), cells);
}

TEST(PipeFormatRoundTrip, AllEmptyMultiCellRow) {
  const std::vector<std::optional<std::string>> cells = {"", "", ""};
  EXPECT_EQ(write_row(cells), "||");
  EXPECT_EQ(read_row("||"), cells);
}

TEST(PipeFormatRoundTrip, AdversarialCells) {
  const std::vector<std::optional<std::string>> cells = {
      "",              // empty
      "\\N",           // literal backslash-N text, must not read back as NULL
      "NULL",          // literal NULL text, must not read back as NULL
      std::nullopt,    // real SQL NULL
      "|",             // separator
      "a|b",           // embedded separator
      "\\",            // lone trailing backslash
      "line1\nline2",  // newline
      "\r\n",          // carriage return
      "tail\\",        // trailing backslash after text
  };
  EXPECT_EQ(read_row(write_row(cells)), cells);
}

TEST(PipeFormatRoundTrip, RandomizedSweep) {
  std::mt19937 rng(20260806);
  const char alphabet[] = {'a', 'b', '|', '\\', '\n', '\r', 'N', '\0', ' '};
  std::uniform_int_distribution<size_t> char_dist(0, sizeof(alphabet) - 1);
  std::uniform_int_distribution<size_t> len_dist(0, 6);
  std::uniform_int_distribution<size_t> cols_dist(1, 5);
  std::uniform_int_distribution<int> null_dist(0, 9);

  for (int iter = 0; iter < 5000; ++iter) {
    std::vector<std::optional<std::string>> cells;
    const size_t cols = cols_dist(rng);
    for (size_t c = 0; c < cols; ++c) {
      if (null_dist(rng) == 0) {
        cells.emplace_back(std::nullopt);
        continue;
      }
      std::string value;
      const size_t len = len_dist(rng);
      for (size_t i = 0; i < len; ++i) {
        value += alphabet[char_dist(rng)];
      }
      cells.emplace_back(std::move(value));
    }
    ASSERT_EQ(read_row(write_row(cells)), cells) << "iteration " << iter;
  }
}

// The eager parser must agree with the writer on the full header+rows layout
TEST(PipeFormatRoundTrip, ParseKeepsTrailingEmptyAndBlankRows) {
  FakeQuery query;

  // Row 1: trailing empty cell. Row 2: all cells empty.
  const std::string raw = "id|note\n1|\n|\n";
  auto result = relx::result::parse(query, raw);
  ASSERT_TRUE(result) << result.error().message;
  ASSERT_EQ(result->size(), 2);

  auto note0 = result->at(0).get<std::string>("note");
  ASSERT_TRUE(note0);
  EXPECT_EQ(*note0, "");

  auto id1 = result->at(1).get<std::string>("id");
  ASSERT_TRUE(id1);
  EXPECT_EQ(*id1, "");
}

TEST(PipeFormatRoundTrip, ParseSingleColumnBlankRow) {
  FakeQuery query;

  // A single-column table with one empty-string row: the row is a blank line
  const std::string raw = "name\n\n";
  auto result = relx::result::parse(query, raw);
  ASSERT_TRUE(result) << result.error().message;
  ASSERT_EQ(result->size(), 1);
  auto name = result->at(0).get<std::string>(0);
  ASSERT_TRUE(name);
  EXPECT_EQ(*name, "");
}

TEST(PipeFormatRoundTrip, ParseRejectsCellCountMismatch) {
  FakeQuery query;

  const std::string raw = "id|name\n1|alice|extra\n";
  auto result = relx::result::parse(query, raw);
  ASSERT_FALSE(result);
  EXPECT_NE(result.error().message.find("cells"), std::string::npos);
}

TEST(PipeFormatRoundTrip, ParseLazyKeepsEmptyHeaderNames) {
  FakeQuery query;

  // Middle column name is empty; the name-to-index mapping must not shift
  const std::string raw = "id||name\n1|x|alice\n";
  auto lazy = relx::result::parse_lazy(query, raw);
  ASSERT_EQ(lazy.column_names().size(), 3);
  EXPECT_EQ(lazy.column_names()[1], "");

  auto row = lazy[0];
  auto name = row.get<std::string>("name");
  ASSERT_TRUE(name) << name.error().message;
  EXPECT_EQ(*name, "alice");
}

TEST(PipeFormatRoundTrip, ParseAndParseLazyAgree) {
  FakeQuery query;

  const std::string raw = "a|b|c\n1||\n\\N|x|\\|\n";
  auto eager = relx::result::parse(query, raw);
  ASSERT_TRUE(eager) << eager.error().message;
  auto lazy = relx::result::parse_lazy(query, raw);

  ASSERT_EQ(eager->size(), lazy.size());
  ASSERT_EQ(eager->column_names(), lazy.column_names());

  for (size_t r = 0; r < eager->size(); ++r) {
    auto lazy_row = lazy[r];
    ASSERT_EQ(eager->at(r).size(), lazy_row.size()) << "row " << r;
    for (size_t c = 0; c < lazy_row.size(); ++c) {
      auto eager_cell = eager->at(r).get_cell(c);
      auto lazy_cell = lazy_row.get_cell(c);
      ASSERT_TRUE(eager_cell);
      ASSERT_TRUE(lazy_cell);
      EXPECT_EQ((*eager_cell)->is_null(), lazy_cell->is_null()) << r << "," << c;
      if (!lazy_cell->is_null()) {
        EXPECT_EQ((*eager_cell)->raw_value(), lazy_cell->get_raw_value()) << r << "," << c;
      }
    }
  }
}

TEST(PipeFormatRoundTrip, LazyRowTrailingEmptyCells) {
  relx::result::LazyRow row(std::string("a|b|"), {"c1", "c2", "c3"});
  ASSERT_EQ(row.size(), 3);
  auto last = row.get<std::string>(2);
  ASSERT_TRUE(last);
  EXPECT_EQ(*last, "");
}

TEST(PipeFormatRoundTrip, LazyCellNullRawValueIsEmpty) {
  relx::result::LazyRow row(std::string("\\N|x"), {"a", "b"});
  auto cell = row.get_cell(0);
  ASSERT_TRUE(cell);
  EXPECT_TRUE(cell->is_null());
  // Unescaping the \N marker would fabricate the value "N"
  EXPECT_EQ(cell->get_raw_value(), "");
}

// Regression: rows handed out by a LazyResultSet once held string_views into the set's
// buffer, dangling when the set was moved or destroyed
TEST(PipeFormatRoundTrip, LazyRowOutlivesResultSet) {
  FakeQuery query;

  std::optional<relx::result::LazyRow> escaped_row;
  {
    auto lazy = relx::result::parse_lazy(query, std::string("id|name\n42|alice\n"));
    escaped_row = lazy[0];
  }  // result set destroyed

  auto name = escaped_row->get<std::string>("name");
  ASSERT_TRUE(name) << name.error().message;
  EXPECT_EQ(*name, "alice");
  auto id = escaped_row->get<int>("id");
  ASSERT_TRUE(id);
  EXPECT_EQ(*id, 42);
}

}  // namespace
