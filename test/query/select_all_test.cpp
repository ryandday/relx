#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <relx/query.hpp>
#include <relx/schema.hpp>

// Define test tables
struct users {
  static constexpr auto table_name = "users";
  relx::schema::column<users, "id", int> id;
  relx::schema::column<users, "name", std::string> name;
  relx::schema::column<users, "email", std::string> email;
  relx::schema::column<users, "age", int> age;
  relx::schema::column<users, "created_at", std::string> created_at;
  relx::schema::column<users, "is_active", bool> is_active;
  relx::schema::column<users, "bio", std::optional<std::string>> bio;
  relx::schema::column<users, "login_count", int> login_count;

  relx::schema::table_primary_key<&users::id> pk;
  relx::schema::unique_constraint<&users::email> unique_email;
};

// Define a second table
struct posts {
  static constexpr auto table_name = "posts";
  relx::schema::column<posts, "id", int> id;
  relx::schema::column<posts, "user_id", int> user_id;
  relx::schema::column<posts, "title", std::string> title;
  relx::schema::column<posts, "content", std::string> content;

  relx::schema::table_primary_key<&posts::id> pk;
  relx::schema::foreign_key<&posts::user_id, &users::id> user_fk;
};

TEST(SelectAllTest, BasicSelectAll) {
  users u;

  // Use select_all with a table instance
  auto query = relx::query::select_all(u);

  // The expected SQL should include all columns but not constraints
  std::string expected_sql = "SELECT * FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(SelectAllTest, SelectAllWithoutInstance) {
  // Use select_all with just the table type
  auto query = relx::query::select_all<users>();

  // The expected SQL should include all columns but not constraints
  std::string expected_sql = "SELECT * FROM users";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(SelectAllTest, SelectAllWithWhere) {
  users u;
  auto query = relx::query::select_all<users>().where(u.age > 18);

  std::string expected_sql = "SELECT * FROM users WHERE (users.age > ?)";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 1);
  EXPECT_EQ(params[0], "18");
}

TEST(SelectAllTest, SelectAllWithJoin) {
  users u;
  posts p;
  auto query = relx::query::select_all<users>().join(p, relx::query::on(u.id == p.user_id));

  std::string expected_sql = "SELECT * FROM users JOIN posts ON (users.id = posts.user_id)";
  EXPECT_EQ(query.to_sql(), expected_sql);
  EXPECT_TRUE(query.bind_params().empty());
}

TEST(SelectAllTest, SelectAllWithAllClauses) {
  users u;
  posts p;
  auto query = relx::query::select_all<users>()
                   .join(p, relx::query::on(u.id == p.user_id))
                   .where(u.age > 18)
                   .group_by(u.id)
                   .having(relx::query::count(p.id) > 5)
                   .order_by(relx::query::desc(u.age))
                   .limit(10)
                   .offset(20);

  std::string expected_sql = "SELECT * FROM users "
                             "JOIN posts ON (users.id = posts.user_id) "
                             "WHERE (users.age > ?) "
                             "GROUP BY users.id "
                             "HAVING (COUNT(posts.id) > ?) "
                             "ORDER BY users.age DESC "
                             "LIMIT ? "
                             "OFFSET ?";
  EXPECT_EQ(query.to_sql(), expected_sql);

  auto params = query.bind_params();
  EXPECT_EQ(params.size(), 4);
  EXPECT_EQ(params[0], "18");
  EXPECT_EQ(params[1], "5");
  EXPECT_EQ(params[2], "10");
  EXPECT_EQ(params[3], "20");
}
