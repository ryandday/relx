#include <chrono>
#include <random>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <relx/connection/sql_utils.hpp>
#include <relx/schema/chrono_traits.hpp>

// Seeded property fuzzing for the parsers relx owns, all of which consume input the
// library does not control. libFuzzer needs Clang, which cannot parse P2996 yet, so
// these are deterministic randomized sweeps; running them under the ASan/UBSan/TSan
// CI jobs is what gives them teeth.

namespace {

constexpr int kIterations = 5000;

std::string random_bytes(std::mt19937& rng, size_t max_len) {
  std::uniform_int_distribution<size_t> len_dist(0, max_len);
  std::uniform_int_distribution<int> byte_dist(0, 255);
  std::string out;
  const size_t len = len_dist(rng);
  out.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    out.push_back(static_cast<char>(byte_dist(rng)));
  }
  return out;
}

std::string random_sqlish(std::mt19937& rng, size_t max_len) {
  // Bias toward the characters the placeholder scanner treats specially
  static constexpr char alphabet[] = "?'\"\\$-/*enNE_ab1 \n\r\t;,()";
  std::uniform_int_distribution<size_t> len_dist(0, max_len);
  std::uniform_int_distribution<size_t> char_dist(0, sizeof(alphabet) - 2);
  std::string out;
  const size_t len = len_dist(rng);
  out.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    out.push_back(alphabet[char_dist(rng)]);
  }
  return out;
}

TEST(ParserFuzz, ConvertPlaceholdersNeverCrashesAndOnlyGrowsBoundedly) {
  std::mt19937 rng(0xC0FFEE);
  for (int i = 0; i < kIterations; ++i) {
    const std::string sql = random_sqlish(rng, 128);
    const std::string converted = relx::connection::sql_utils::convert_placeholders_to_postgresql(
        sql);
    // Each ? can become at most $NNN; anything else must not grow
    EXPECT_LE(converted.size(), sql.size() * 4 + 8) << sql;
  }
}

TEST(ParserFuzz, DecodeBinaryCellHandlesArbitraryBytes) {
  using relx::connection::sql_utils::decode_binary_cell_for_testing;

  std::mt19937 rng(0xDEC0DE);
  // The OIDs the decoder dispatches on, plus a user-defined one and a bogus one
  constexpr unsigned int oids[] = {16,   20,   21,   23,   25,   700,  701,   1042,
                                   1043, 1082, 1114, 1184, 1700, 2950, 16385, 4242};
  std::uniform_int_distribution<size_t> oid_dist(0, std::size(oids) - 1);

  for (int i = 0; i < kIterations; ++i) {
    const std::string bytes = random_bytes(rng, 64);
    // Must return a value or an error - never crash, never read out of bounds
    auto decoded = decode_binary_cell_for_testing(oids[oid_dist(rng)], bytes.data(),
                                                  static_cast<int>(bytes.size()));
    (void)decoded;
  }
}

TEST(ParserFuzz, ChronoFromSqlStringThrowsOrParsesArbitraryText) {
  using tp_traits = relx::schema::column_traits<std::chrono::system_clock::time_point>;
  using ymd_traits = relx::schema::column_traits<std::chrono::year_month_day>;

  std::mt19937 rng(0xDA7E5);
  for (int i = 0; i < kIterations; ++i) {
    // Digit/separator-biased strings hit the parser's deep paths more often
    static constexpr char alphabet[] = "0123456789-+:.TZ 'i nf";
    std::uniform_int_distribution<size_t> len_dist(0, 40);
    std::uniform_int_distribution<size_t> char_dist(0, sizeof(alphabet) - 2);
    std::string text;
    const size_t len = len_dist(rng);
    for (size_t j = 0; j < len; ++j) {
      text.push_back(alphabet[char_dist(rng)]);
    }

    try {
      (void)tp_traits::from_sql_string(text);
    } catch (const std::invalid_argument&) {
      // rejecting is fine; crashing or UB is not
    }
    try {
      (void)ymd_traits::from_sql_string(text);
    } catch (const std::invalid_argument&) {
    }
  }
}

}  // namespace
