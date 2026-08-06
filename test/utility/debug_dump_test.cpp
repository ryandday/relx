#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/debug.hpp>

// relx::debug::dump pretty-printer

namespace {

enum class Color { red, green };

struct Inner {
  int x;
  std::optional<std::string> label;
};

struct Outer {
  int id;
  std::string name;
  bool active;
  Color color;
  double ratio;
  std::optional<int> rank;
  std::vector<int> scores;
  Inner inner;
};

}  // namespace

TEST(DebugDumpTest, RendersScalarsAndStrings) {
  const Inner inner{.x = 7, .label = "tag"};
  const std::string out = relx::debug::dump(inner);
  EXPECT_NE(out.find("x: 7"), std::string::npos) << out;
  EXPECT_NE(out.find("label: \"tag\""), std::string::npos) << out;
}

TEST(DebugDumpTest, RendersNullOptionalsEnumsVectorsAndNesting) {
  const Outer outer{.id = 1,
                    .name = "ada",
                    .active = true,
                    .color = Color::green,
                    .ratio = 2.5,
                    .rank = std::nullopt,
                    .scores = {10, 20},
                    .inner = {.x = -3, .label = std::nullopt}};
  const std::string out = relx::debug::dump(outer);
  EXPECT_NE(out.find("name: \"ada\""), std::string::npos) << out;
  EXPECT_NE(out.find("active: true"), std::string::npos) << out;
  EXPECT_NE(out.find("color: green"), std::string::npos) << out;
  EXPECT_NE(out.find("ratio: 2.5"), std::string::npos) << out;
  EXPECT_NE(out.find("rank: null"), std::string::npos) << out;
  EXPECT_NE(out.find("scores: [10, 20]"), std::string::npos) << out;
  EXPECT_NE(out.find("x: -3"), std::string::npos) << out;
  EXPECT_NE(out.find("label: null"), std::string::npos) << out;
}

TEST(DebugDumpTest, EnumOutsideEnumerationRendersNumeric) {
  const std::string out = relx::debug::dump(static_cast<Color>(42));
  EXPECT_EQ(out, "<enum:42>");
}
