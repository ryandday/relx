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

// clang-format on

}  // namespace
