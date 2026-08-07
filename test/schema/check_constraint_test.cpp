#include <string>

#include <gtest/gtest.h>
#include <relx/schema.hpp>

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

// Single-column CHECKs are column modifier annotations; constraints that span
// columns are struct-level ann::check annotations.
struct [[=relx::table("items"),
        =relx::ann::check("category IN ('electronics', 'books', 'clothing')"),
        =relx::ann::check("(price < 100.0 AND category = 'books') OR category != 'books'")]] Item {
  [[=relx::ann::pk]] int id;
  std::string item_name;
  [[=relx::schema::check<"price > 0">{}]] double price;
  [[=relx::schema::check<"quantity >= 0">{}]] int quantity;
  std::string category;
};

TEST(CheckConstraintTest, ColumnModifierChecks) {
  constexpr auto ddl = relx::create_table_sql<Item>().to_sql();
  EXPECT_NE(ddl.find("price DOUBLE PRECISION NOT NULL CHECK(price > 0)"), std::string_view::npos);
  EXPECT_NE(ddl.find("quantity INTEGER NOT NULL CHECK(quantity >= 0)"), std::string_view::npos);
}

TEST(CheckConstraintTest, TableLevelChecks) {
  EXPECT_EQ(relx::schema::table_constraints_sql<Item>(),
            "CHECK (category IN ('electronics', 'books', 'clothing')),\n"
            "CHECK ((price < 100.0 AND category = 'books') OR category != 'books')");
}

// Special characters survive the consteval round-trip into the constraint SQL
struct [[=relx::table("special_items"),
        =relx::ann::check("item_name LIKE '%special''s item%'"),
        =relx::ann::check("item_name LIKE '%\\special\\%' OR item_name LIKE '%\"quoted\"%'"),
        =relx::ann::check("item_name LIKE '%O''Brien''s%' OR item_name LIKE '%100\\%%'")]]
SpecialItem {
  std::string item_name;
};

TEST(CheckConstraintTest, SpecialCharacters) {
  EXPECT_EQ(relx::schema::table_constraints_sql<SpecialItem>(),
            "CHECK (item_name LIKE '%special''s item%'),\n"
            "CHECK (item_name LIKE '%\\special\\%' OR item_name LIKE '%\"quoted\"%'),\n"
            "CHECK (item_name LIKE '%O''Brien''s%' OR item_name LIKE '%100\\%%')");
}

// Named constraints render as CONSTRAINT <name> CHECK (...)
struct [[=relx::table("named_items"),
        =relx::ann::check("price > 0").named("positive_price"),
        =relx::ann::check("quantity * price >= 1000").named("min_order_value"),
        =relx::ann::check("price > 100").named("premium_price_$")]] NamedItem {
  double price;
  int quantity;
};

TEST(CheckConstraintTest, NamedConstraints) {
  // Names quote like every other identifier: premium_price_$ needs quoting, or
  // PostgreSQL would reject/fold it and DROP CONSTRAINT would target the wrong name
  EXPECT_EQ(relx::schema::table_constraints_sql<NamedItem>(),
            "CONSTRAINT positive_price CHECK (price > 0),\n"
            "CONSTRAINT min_order_value CHECK (quantity * price >= 1000),\n"
            "CONSTRAINT \"premium_price_$\" CHECK (price > 100)");
}

// clang-format on

}  // namespace
