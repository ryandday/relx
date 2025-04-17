#include <gtest/gtest.h>
#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

// Define test tables
struct users {
    static constexpr auto table_name = "users";
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::column<"age", int> age;
    sqllib::schema::column<"bio", std::optional<std::string>> bio;
    
    sqllib::schema::primary_key<&users::id> pk;
    sqllib::schema::unique_constraint<&users::email> unique_email;
};

struct posts {
    static constexpr auto table_name = "posts";
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    sqllib::schema::column<"created_at", std::string> created_at;
    
    sqllib::schema::primary_key<&posts::id> pk;
    sqllib::schema::foreign_key<&posts::user_id, &users::id> user_fk;
};

TEST(QueryTest, SimpleSelect) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name, u.email)
        .from(u);
    
    std::string expected_sql = "SELECT id, name, email FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(QueryTest, SelectWithCondition) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(u.age > 18);
    
    std::string expected_sql = "SELECT id, name FROM users WHERE (age > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "18");
}

TEST(QueryTest, SelectWithJoin) {
    users u;
    posts p;
    
    auto query = sqllib::query::select(u.name, p.title)
        .from(u)
        .join(p, sqllib::query::on(u.id == p.user_id));
    
    std::string expected_sql = "SELECT name, title FROM users JOIN posts ON (id = user_id)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(QueryTest, SelectWithMultipleConditions) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(u.age >= 18 && 
               u.name != "");
    
    std::string expected_sql = "SELECT id, name FROM users WHERE ((age >= ?) AND (name != ?))";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "");
}

TEST(QueryTest, SelectWithOrderByAndLimit) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name)
        .from(u)
        .order_by(sqllib::query::desc(u.name))
        .limit(10);
    
    std::string expected_sql = "SELECT id, name FROM users ORDER BY name DESC LIMIT ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "10");
}

TEST(QueryTest, SelectWithAggregateFunction) {
    users u;
    
    auto query = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::count_all(), "user_count"),
        sqllib::query::as(sqllib::query::avg(u.age), "average_age")
    ).from(u);
    
    std::string expected_sql = "SELECT COUNT(*) AS user_count, AVG(age) AS average_age FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    EXPECT_TRUE(query.bind_params().empty());
}

TEST(QueryTest, SelectWithGroupByAndHaving) {
    users u;
    posts p;
    
    auto query = sqllib::query::select_expr(
        u.id, 
        sqllib::query::as(sqllib::query::count(p.id), "post_count"))
        .from(u)
        .join(p, sqllib::query::on(u.id == p.user_id))
        .group_by(u.id)
        .having(sqllib::query::count(p.id) > 5);
    
    std::string expected_sql = "SELECT id, COUNT(id) AS post_count FROM users JOIN posts ON (id = user_id) GROUP BY id HAVING (COUNT(id) > ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "5");
}

TEST(QueryTest, SelectWithInCondition) {
    users u;
    
    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
    auto query = sqllib::query::select(u.id, u.email)
        .from(u)
        .where(sqllib::query::in(u.name, names));
    
    std::string expected_sql = "SELECT id, email FROM users WHERE name IN (?, ?, ?)";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 3);
    EXPECT_EQ(params[0], "Alice");
    EXPECT_EQ(params[1], "Bob");
    EXPECT_EQ(params[2], "Charlie");
}

TEST(QueryTest, SelectWithLikeCondition) {
    users u;
    
    auto query = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::like(u.email, "%@example.com"));
    
    std::string expected_sql = "SELECT id, name FROM users WHERE email LIKE ?";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0], "%@example.com");
}

TEST(QueryTest, SelectWithCaseExpression) {
    users u;
    
    auto case_expr = sqllib::query::case_()
        .when(u.age < 18, "Minor")
        .when(u.age < 65, "Adult")
        .else_("Senior")
        .build();
    
    // First check the case expression directly
    std::cout << "Case SQL: " << case_expr.to_sql() << std::endl;
    auto case_params = case_expr.bind_params();
    std::cout << "Case params (" << case_params.size() << "): ";
    for (size_t i = 0; i < case_params.size(); ++i) {
        std::cout << "[" << i << "]=" << case_params[i] << " ";
    }
    std::cout << std::endl;
    
    // Then check with the alias
    auto aliased_case = sqllib::query::as(std::move(case_expr), "age_group");
    std::cout << "Aliased SQL: " << aliased_case.to_sql() << std::endl;
    auto alias_params = aliased_case.bind_params();
    std::cout << "Alias params (" << alias_params.size() << "): ";
    for (size_t i = 0; i < alias_params.size(); ++i) {
        std::cout << "[" << i << "]=" << alias_params[i] << " ";
    }
    std::cout << std::endl;
    
    // Finally the full query
    auto query = sqllib::query::select_expr(
        u.name, 
        std::move(aliased_case))
        .from(u);
    
    std::cout << "Query SQL: " << query.to_sql() << std::endl;
    
    std::string expected_sql = "SELECT name, CASE WHEN (age < ?) THEN ? WHEN (age < ?) THEN ? ELSE ? END AS age_group FROM users";
    EXPECT_EQ(query.to_sql(), expected_sql);
    
    auto params = query.bind_params();
    std::cout << "Query params (" << params.size() << "): ";
    for (size_t i = 0; i < params.size(); ++i) {
        std::cout << "[" << i << "]=" << params[i] << " ";
    }
    std::cout << std::endl;
    
    // The validation is valid now that we've fixed the issues
    EXPECT_EQ(params.size(), 5);
    EXPECT_EQ(params[0], "18");
    EXPECT_EQ(params[1], "Minor");
    EXPECT_EQ(params[2], "65");
    EXPECT_EQ(params[3], "Adult");
    EXPECT_EQ(params[4], "Senior");
}

TEST(QueryTest, SimpleCaseWithoutDuplicateParams) {
    users u;
    
    // Create a simple value-only condition first
    auto value_query = sqllib::query::select_expr(
        sqllib::query::val(42)
    );
    
    std::cout << "Value SQL: " << value_query.to_sql() << std::endl;
    auto value_params = value_query.bind_params();
    std::cout << "Value params (" << value_params.size() << "): ";
    for (size_t i = 0; i < value_params.size(); ++i) {
        std::cout << "[" << i << "]=" << value_params[i] << " ";
    }
    std::cout << std::endl;
    
    EXPECT_EQ(value_params.size(), 1);
    EXPECT_EQ(value_params[0], "42");
}

TEST(QueryTest, DebugSelectExprDuplicateParams) {
    // Test a direct value parameter
    auto single_val = sqllib::query::val(123);
    std::cout << "Single value SQL: " << single_val.to_sql() << std::endl;
    auto single_params = single_val.bind_params();
    std::cout << "Single value params (" << single_params.size() << "): ";
    for (size_t i = 0; i < single_params.size(); ++i) {
        std::cout << "[" << i << "]=" << single_params[i] << " ";
    }
    std::cout << std::endl;
    
    // Test making a tuple with the value
    auto value_tuple = std::make_tuple(single_val);
    std::cout << "Tuple contents: " << std::get<0>(value_tuple).to_sql() << std::endl;
    
    // Test the SelectQuery with the value directly
    auto direct_query = sqllib::query::SelectQuery<std::tuple<decltype(single_val)>>(
        std::make_tuple(single_val)
    );
    
    std::cout << "Direct query SQL: " << direct_query.to_sql() << std::endl;
    auto direct_params = direct_query.bind_params();
    std::cout << "Direct query params (" << direct_params.size() << "): ";
    for (size_t i = 0; i < direct_params.size(); ++i) {
        std::cout << "[" << i << "]=" << direct_params[i] << " ";
    }
    std::cout << std::endl;
    
    // Now test through the select_expr helper function
    auto select_query = sqllib::query::select_expr(single_val);
    
    std::cout << "Select query SQL: " << select_query.to_sql() << std::endl;
    auto select_params = select_query.bind_params();
    std::cout << "Select query params (" << select_params.size() << "): ";
    for (size_t i = 0; i < select_params.size(); ++i) {
        std::cout << "[" << i << "]=" << select_params[i] << " ";
    }
    std::cout << std::endl;
} 