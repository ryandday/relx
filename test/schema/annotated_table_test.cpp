#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/migrations.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

namespace {

// clang-format off: annotation/reflection syntax is not yet understood by clang-format 20

struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
  std::string username;
  [[=relx::ann::unique]] std::string email;
  [[=relx::default_value<true>{}]] bool active;
  [[=relx::default_value<18>{}]] int age;
  [[=relx::string_default<"CURRENT_TIMESTAMP", true>{}]] std::string created_at;
  std::optional<std::string> bio;
};
inline constexpr auto users = relx::t<Users>;

struct [[=relx::table("posts")]] Posts {
  [[=relx::ann::pk]] int id;
  [[=relx::ann::fk<^^Users::id>]] int user_id;
  std::string title;
};
inline constexpr auto posts = relx::t<Posts>;

// No table annotation: the struct identifier is the table name
struct Unannotated {
  int id;
};

TEST(AnnotatedTableTest, TableNameFromAnnotation) {
  EXPECT_EQ(relx::table_name_of<Users>(), "users");
  EXPECT_EQ(users.table_name, "users");
}

TEST(AnnotatedTableTest, TableNameFallsBackToIdentifier) {
  EXPECT_EQ(relx::table_name_of<Unannotated>(), "Unannotated");
}

TEST(AnnotatedTableTest, ColumnMembersDeriveFromStructFields) {
  using IdColumn = std::remove_cvref_t<decltype(users.id)>;
  EXPECT_EQ(std::string_view(IdColumn::name), "id");
  EXPECT_EQ(std::string_view(IdColumn::sql_type), "INTEGER");
  EXPECT_FALSE(IdColumn::nullable);

  using BioColumn = std::remove_cvref_t<decltype(users.bio)>;
  EXPECT_EQ(std::string_view(BioColumn::name), "bio");
  EXPECT_TRUE(BioColumn::nullable);
}

TEST(AnnotatedTableTest, ColumnSqlDefinitions) {
  EXPECT_EQ(users.id.sql_definition(), "id INTEGER NOT NULL PRIMARY KEY");
  EXPECT_EQ(users.username.sql_definition(), "username TEXT NOT NULL");
  EXPECT_EQ(users.email.sql_definition(), "email TEXT NOT NULL UNIQUE");
  EXPECT_EQ(users.active.sql_definition(), "active BOOLEAN NOT NULL DEFAULT true");
  EXPECT_EQ(users.age.sql_definition(), "age INTEGER NOT NULL DEFAULT 18");
  EXPECT_EQ(users.created_at.sql_definition(),
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP");
  EXPECT_EQ(users.bio.sql_definition(), "bio TEXT");
}

TEST(AnnotatedTableTest, ForeignKeyAnnotation) {
  EXPECT_EQ(posts.user_id.sql_definition(), "user_id INTEGER NOT NULL REFERENCES users(id)");
}

TEST(AnnotatedTableTest, CreateTableSql) {
  auto sql = relx::create_table(users).to_sql();
  EXPECT_EQ(sql,
            "CREATE TABLE users (\n"
            "id INTEGER NOT NULL PRIMARY KEY,\n"
            "username TEXT NOT NULL,\n"
            "email TEXT NOT NULL UNIQUE,\n"
            "active BOOLEAN NOT NULL DEFAULT true,\n"
            "age INTEGER NOT NULL DEFAULT 18,\n"
            "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
            "bio TEXT\n"
            ");");
}

TEST(AnnotatedTableTest, CreateTableIfNotExists) {
  auto sql = relx::create_table(posts).if_not_exists().to_sql();
  EXPECT_EQ(sql,
            "CREATE TABLE IF NOT EXISTS posts (\n"
            "id INTEGER NOT NULL PRIMARY KEY,\n"
            "user_id INTEGER NOT NULL REFERENCES users(id),\n"
            "title TEXT NOT NULL\n"
            ");");
}

TEST(AnnotatedTableTest, DropTableSql) {
  auto sql = relx::drop_table(users).if_exists().to_sql();
  EXPECT_EQ(sql, "DROP TABLE IF EXISTS users;");
}

TEST(AnnotatedTableTest, SelectWhereSql) {
  auto query = relx::query::select(users.id, users.username).from(users).where(users.id == 42);
  EXPECT_EQ(query.to_sql(), "SELECT users.id, users.username FROM users WHERE (users.id = ?)");
  ASSERT_EQ(query.bind_params().size(), 1);
  EXPECT_EQ(query.bind_params()[0], "42");
}

TEST(AnnotatedTableTest, JoinSql) {
  auto query = relx::query::select(users.username, posts.title)
                   .from(users)
                   .join(posts, relx::query::on(users.id == posts.user_id));
  EXPECT_EQ(query.to_sql(),
            "SELECT users.username, posts.title FROM users JOIN posts ON (users.id = "
            "posts.user_id)");
}

TEST(AnnotatedTableTest, InsertSql) {
  auto query = relx::query::insert_into(users)
                   .columns(users.username, users.email)
                   .values("jane", "jane@example.com");
  EXPECT_EQ(query.to_sql(), "INSERT INTO users (username, email) VALUES (?, ?)");
  ASSERT_EQ(query.bind_params().size(), 2);
}

TEST(AnnotatedTableTest, UpdateSql) {
  auto query = relx::query::update(users).set(users.email, "new@example.com").where(users.id == 1);
  EXPECT_EQ(query.to_sql(), "UPDATE users SET email = ? WHERE (users.id = ?)");
}

TEST(AnnotatedTableTest, DeleteSql) {
  auto query = relx::query::delete_from(users).where(users.id == 1);
  EXPECT_EQ(query.to_sql(), "DELETE FROM users WHERE (users.id = ?)");
}

// DDL built entirely at compile time - the whole CREATE TABLE is a static_assert
TEST(AnnotatedTableTest, ConstevalCreateTableSql) {
  static_assert(relx::create_table_sql<Posts>().to_sql() ==
                "CREATE TABLE posts (\n"
                "id INTEGER NOT NULL PRIMARY KEY,\n"
                "user_id INTEGER NOT NULL REFERENCES users(id),\n"
                "title TEXT NOT NULL\n"
                ");");
  static_assert(relx::create_table_sql<Posts>().if_not_exists().to_sql().starts_with(
      "CREATE TABLE IF NOT EXISTS posts"));
  static_assert(relx::drop_table_sql<Posts>().if_exists().to_sql() ==
                "DROP TABLE IF EXISTS posts;");
  static_assert(relx::drop_table_sql<Posts>().if_exists().cascade().to_sql() ==
                "DROP TABLE IF EXISTS posts CASCADE;");

  // consteval and runtime builders agree
  constexpr auto posts_ddl = relx::create_table_sql<Posts>().if_not_exists().to_sql();
  EXPECT_EQ(posts_ddl, relx::create_table(posts).if_not_exists().to_sql());
  constexpr auto users_ddl = relx::create_table_sql<Users>().to_sql();
  EXPECT_EQ(users_ddl, relx::create_table(users).to_sql());
}

// Field iteration must walk the define_aggregate base of table_ref, or migrations
// silently see zero columns and a diff would emit DROPs for everything
TEST(AnnotatedTableTest, MigrationsSeeSynthesizedColumns) {
  static_assert(relx::refl::field_count<relx::table_ref<Users>>() == 7);

  auto metadata = relx::migrations::extract_table_metadata(users);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->columns.size(), 7);
  EXPECT_TRUE(metadata->columns.contains("id"));
  EXPECT_TRUE(metadata->columns.contains("bio"));
  EXPECT_TRUE(metadata->columns["bio"].nullable);
}

// relx::c<^^T::member> is the standalone escape hatch when no table object is in scope

TEST(AnnotatedTableTest, StandaloneColumnRef) {
  EXPECT_EQ(relx::c<^^Users::id>.sql_definition(), "id INTEGER NOT NULL PRIMARY KEY");
  auto query = relx::query::select(relx::c<^^Users::id>)
                   .from(relx::t<Users>)
                   .where(relx::c<^^Users::id> == 7);
  EXPECT_EQ(query.to_sql(), "SELECT users.id FROM users WHERE (users.id = ?)");
}

// Struct-level annotations: composite keys, indexes, table-level checks

struct [[=relx::table("order_items"),
        =relx::ann::composite_pk("order_id", "product_id"),
        =relx::ann::composite_unique("region", "external_ref"),
        =relx::ann::index_on("customer_id", "created_at"),
        =relx::ann::index_on("external_ref").unique(),
        =relx::ann::check("quantity > 0").named("positive_quantity")]] OrderItems {
  int order_id;
  int product_id;
  int customer_id;
  std::string region;
  std::string external_ref;
  std::string created_at;
  int quantity;
};

struct [[=relx::table("shipments"),
        =relx::ann::composite_fk<^^OrderItems::order_id, ^^OrderItems::product_id>(
            "order_id", "product_id")]] Shipments {
  [[=relx::ann::pk]] int id;
  int order_id;
  int product_id;
};

TEST(AnnotatedTableTest, TableLevelConstraintDefinitions) {
  EXPECT_EQ(relx::schema::table_constraints_sql<OrderItems>(),
            "PRIMARY KEY (order_id, product_id),\n"
            "UNIQUE (region, external_ref),\n"
            "CONSTRAINT positive_quantity CHECK (quantity > 0)");
}

TEST(AnnotatedTableTest, CompositeForeignKey) {
  EXPECT_EQ(relx::schema::table_constraints_sql<Shipments>(),
            "FOREIGN KEY (order_id, product_id) REFERENCES order_items (order_id, product_id)");
}

TEST(AnnotatedTableTest, CreateTableIncludesTableConstraints) {
  constexpr auto ddl = relx::create_table_sql<OrderItems>().to_sql();
  static_assert(ddl ==
                "CREATE TABLE order_items (\n"
                "order_id INTEGER NOT NULL,\n"
                "product_id INTEGER NOT NULL,\n"
                "customer_id INTEGER NOT NULL,\n"
                "region TEXT NOT NULL,\n"
                "external_ref TEXT NOT NULL,\n"
                "created_at TEXT NOT NULL,\n"
                "quantity INTEGER NOT NULL,\n"
                "PRIMARY KEY (order_id, product_id),\n"
                "UNIQUE (region, external_ref),\n"
                "CONSTRAINT positive_quantity CHECK (quantity > 0)\n"
                ");");
  SUCCEED();
}

TEST(AnnotatedTableTest, RuntimeCreateTableIncludesTableConstraints) {
  auto sql = relx::create_table(relx::t<OrderItems>).to_sql();
  EXPECT_NE(sql.find("PRIMARY KEY (order_id, product_id)"), std::string::npos);
  EXPECT_NE(sql.find("UNIQUE (region, external_ref)"), std::string::npos);
}

TEST(AnnotatedTableTest, IndexAnnotationsBecomeCreateIndexStatements) {
  constexpr auto stmts = relx::create_indexes_sql<OrderItems>();
  static_assert(stmts.size() == 2);
  EXPECT_EQ(stmts[0], "CREATE INDEX order_items_customer_id_created_at_idx ON order_items "
                      "(customer_id, created_at)");
  EXPECT_EQ(stmts[1],
            "CREATE UNIQUE INDEX order_items_external_ref_idx ON order_items (external_ref)");
}

// Migrations see table-level annotations: two versions of the same table, the diff
// carries the added constraint and index

struct [[=relx::table("shipping_rates"),
        =relx::ann::composite_pk("region", "carrier")]] ShippingRatesV1 {
  std::string region;
  std::string carrier;
  double rate;
};

struct [[=relx::table("shipping_rates"),
        =relx::ann::composite_pk("region", "carrier"),
        =relx::ann::composite_unique("carrier", "rate"),
        =relx::ann::index_on("carrier")]] ShippingRatesV2 {
  std::string region;
  std::string carrier;
  double rate;
};

TEST(AnnotatedTableTest, MetadataIncludesTableLevelAnnotations) {
  auto metadata = relx::migrations::extract_table_metadata(relx::t<ShippingRatesV2>);
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->constraints.size(), 3);
  EXPECT_EQ(metadata->constraints.at("shipping_rates_pk").sql_definition,
            "PRIMARY KEY (region, carrier)");
  EXPECT_EQ(metadata->constraints.at("shipping_rates_carrier_idx").type, "INDEX");
  EXPECT_EQ(metadata->constraints.at("shipping_rates_carrier_idx").sql_definition,
            "INDEX shipping_rates_carrier_idx ON shipping_rates (carrier)");
}

TEST(AnnotatedTableTest, MigrationDiffSeesAnnotationConstraints) {
  auto migration = relx::migrations::generate_migration(relx::t<ShippingRatesV1>,
                                                        relx::t<ShippingRatesV2>);
  ASSERT_TRUE(migration) << migration.error().format();
  auto forward = migration->forward_sql();
  ASSERT_TRUE(forward);

  std::string all_sql;
  for (const auto& sql : *forward) {
    all_sql += sql + "\n";
  }
  EXPECT_NE(all_sql.find("ALTER TABLE shipping_rates ADD UNIQUE (carrier, rate);"),
            std::string::npos)
      << all_sql;
  EXPECT_NE(all_sql.find("CREATE INDEX shipping_rates_carrier_idx ON shipping_rates (carrier);"),
            std::string::npos)
      << all_sql;
}

// clang-format on

}  // namespace
