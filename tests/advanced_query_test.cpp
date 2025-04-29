#include <gtest/gtest.h>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <array>

// Define test tables for our advanced query tests
struct Users {
    static constexpr auto table_name = "users";
    relx::schema::column<Users, "id", int> id;
    relx::schema::column<Users, "name", std::string> name;
    relx::schema::column<Users, "email", std::string> email;
    relx::schema::column<Users, "age", int> age;
    relx::schema::column<Users, "is_active", bool> is_active;
    relx::schema::column<Users, "department_id", int> department_id;
};

struct Posts {
    static constexpr auto table_name = "posts";
    relx::schema::column<Posts, "id", int> id;
    relx::schema::column<Posts, "user_id", int> user_id;
    relx::schema::column<Posts, "title", std::string> title;
    relx::schema::column<Posts, "content", std::string> content;
    relx::schema::column<Posts, "views", int> views;
    relx::schema::column<Posts, "created_at", std::string> created_at;
};

struct Comments {
    static constexpr auto table_name = "comments";
    relx::schema::column<Comments,  "id", int> id;
    relx::schema::column<Comments, "post_id", int> post_id;
    relx::schema::column<Comments, "user_id", int> user_id; 
    relx::schema::column<Comments, "content", std::string> content;
    relx::schema::column<Comments, "created_at", std::string> created_at;
};

struct Departments {
    static constexpr auto table_name = "departments";
    relx::schema::column<Departments, "id", int> id;
    relx::schema::column<Departments, "name", std::string> name;
    relx::schema::column<Departments, "budget", double> budget;
};

// Utility function to create sample raw results for testing
std::string create_raw_results(const std::vector<std::string>& headers, 
                               const std::vector<std::vector<std::string>>& rows) {
    std::string result;
    
    // Headers
    for (size_t i = 0; i < headers.size(); ++i) {
        result += headers[i];
        if (i < headers.size() - 1) {
            result += "|";
        }
    }
    result += "\n";
    
    // Rows
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            result += row[i];
            if (i < row.size() - 1) {
                result += "|";
            }
        }
        result += "\n";
    }
    
    return result;
}

// Test fixture for advanced query and result processing tests
class AdvancedQueryTest : public ::testing::Test {
protected:
    Users users;
    Posts posts;
    Comments comments;
    Departments departments;
};

// Test a join between Users and Posts
TEST_F(AdvancedQueryTest, JoinTest) {
    // Create a query that joins users and posts
    auto query = relx::query::select(
        users.id, users.name, posts.id, posts.title
    ).from(users)
     .join(posts, relx::query::on(
         users.id == posts.user_id
     ));
    
    // Check the SQL generated
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name, posts.id, posts.title FROM users JOIN posts ON (users.id = posts.user_id)");
    
    // Create some sample raw results - note that columns are aliased to avoid duplicate names
    std::vector<std::string> headers = {"id", "name", "id", "title"};
    std::vector<std::vector<std::string>> rows = {
        {"1", "John Doe", "101", "First Post"},
        {"1", "John Doe", "102", "Second Post"},
        {"2", "Jane Smith", "201", "Hello World"}
    };
    std::string raw_results = create_raw_results(headers, rows);
    
    // Parse the results
    auto result = relx::result::parse(query, raw_results);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    EXPECT_EQ(3, results.size());
    
    // Get values from the joined result
    const auto& first_row = results.at(0);
    
    // Get user id (first column)
    auto user_id = first_row.get<int>(0);
    ASSERT_TRUE(user_id) << user_id.error().message;
    EXPECT_EQ(1, *user_id);
    
    // Get user name (second column)
    auto user_name = first_row.get<std::string>(1);
    ASSERT_TRUE(user_name) << user_name.error().message;
    EXPECT_EQ("John Doe", *user_name);
    
    // Get post id (third column)
    auto post_id = first_row.get<int>(2);
    ASSERT_TRUE(post_id) << post_id.error().message;
    EXPECT_EQ(101, *post_id);
    
    // Get post title (fourth column)
    auto post_title = first_row.get<std::string>(3);
    ASSERT_TRUE(post_title) << post_title.error().message;
    EXPECT_EQ("First Post", *post_title);
    
    // Define a struct to hold the joined data
    struct UserPost {
        int user_id;
        std::string user_name;
        int post_id;
        std::string post_title;
    };
    
    // Transform the results into a vector of UserPost objects
    auto user_posts = results.transform<UserPost>([](const relx::result::Row& row) -> relx::result::ResultProcessingResult<UserPost> {
        auto user_id = row.get<int>(0);
        auto user_name = row.get<std::string>(1);
        auto post_id = row.get<int>(2);
        auto post_title = row.get<std::string>(3);
        
        if (!user_id || !user_name || !post_id || !post_title) {
            return std::unexpected(relx::result::ResultError{"Failed to extract user post data"});
        }
        
        return UserPost{*user_id, *user_name, *post_id, *post_title};
    });
    
    ASSERT_EQ(3, user_posts.size());
    EXPECT_EQ(1, user_posts[0].user_id);
    EXPECT_EQ("John Doe", user_posts[0].user_name);
    EXPECT_EQ(101, user_posts[0].post_id);
    EXPECT_EQ("First Post", user_posts[0].post_title);
}

// Test a query with a WHERE clause
TEST_F(AdvancedQueryTest, WhereClauseTest) {
    // Create a query that filters users by age and active status
    auto query = relx::query::select(
        users.id, users.name, users.age
    ).from(users)
     .where(
         users.age > 30 && users.is_active == true
     );
    
    // Check the SQL generated
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name, users.age FROM users WHERE ((users.age > ?) AND (users.is_active = ?))");
    
    // Check the parameters
    auto params = query.bind_params();
    EXPECT_EQ(2, params.size());
    EXPECT_EQ("30", params[0]);
    EXPECT_EQ("1", params[1]);  // true is represented as 1
    
    // Create some sample raw results
    std::vector<std::string> headers = {"id", "name", "age"};
    std::vector<std::vector<std::string>> rows = {
        {"3", "Bob Johnson", "45"},
        {"5", "Maria Garcia", "38"}
    };
    std::string raw_results = create_raw_results(headers, rows);
    
    // Parse the results
    auto result = relx::result::parse(query, raw_results);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    EXPECT_EQ(2, results.size());
    
    // Check the filtered results
    const auto& first_row = results.at(0);
    auto age = first_row.get<int>("age");
    ASSERT_TRUE(age) << age.error().message;
    EXPECT_GT(*age, 30);
    
    // Use structured binding to process all rows
    for (const auto& row : results) {
        auto age_val = row.get<int>("age");
        ASSERT_TRUE(age_val) << age_val.error().message;
        EXPECT_GT(*age_val, 30);
    }
}

// Test a query with a GROUP BY clause and aggregation
TEST_F(AdvancedQueryTest, GroupByTest) {
    // Create a query that counts posts per user
    auto query = relx::query::select_expr(
        users.id,
        users.name,
        relx::query::as(relx::query::count(posts.id), "post_count")
    ).from(users)
     .join(posts, relx::query::on(
         users.id == posts.user_id
     ))
     .group_by(users.id, users.name)
     .having(relx::query::count(posts.id) > 1);
    
    // Check the SQL generated
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name, COUNT(posts.id) AS post_count FROM users JOIN posts ON (users.id = posts.user_id) GROUP BY users.id, users.name HAVING (COUNT(posts.id) > ?)");
    
    // Check the parameters
    auto params = query.bind_params();
    EXPECT_EQ(1, params.size());
    EXPECT_EQ("1", params[0]);
    
    // Create some sample raw results
    std::vector<std::string> headers = {"id", "name", "post_count"};
    std::vector<std::vector<std::string>> rows = {
        {"1", "John Doe", "5"},
        {"3", "Bob Johnson", "3"}
    };
    std::string raw_results = create_raw_results(headers, rows);
    
    // Parse the results
    auto result = relx::result::parse(query, raw_results);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    EXPECT_EQ(2, results.size());
    
    // Check the grouped and aggregated results
    struct UserPostCount {
        int user_id;
        std::string name;
        int post_count;
    };
    
    auto user_post_counts = results.transform<UserPostCount>([](const relx::result::Row& row) -> relx::result::ResultProcessingResult<UserPostCount> {
        auto user_id = row.get<int>("id");
        auto name = row.get<std::string>("name");
        auto post_count = row.get<int>("post_count");
        
        if (!user_id || !name || !post_count) {
            return std::unexpected(relx::result::ResultError{"Failed to extract user post count data"});
        }
        
        return UserPostCount{*user_id, *name, *post_count};
    });
    
    ASSERT_EQ(2, user_post_counts.size());
    EXPECT_GT(user_post_counts[0].post_count, 1);
    EXPECT_GT(user_post_counts[1].post_count, 1);
}

// Test a complex query with multiple joins, where conditions, and aggregation
TEST_F(AdvancedQueryTest, ComplexQueryTest) {
    // Create a complex query:
    // - Join Users, Posts, Comments, and Departments
    // - Filter by department and post views
    // - Group by department and count users and posts
    // - Order by user count
    auto query = relx::query::select_expr(
        departments.name,
        relx::query::as(relx::query::count_distinct(users.id), "user_count"),
        relx::query::as(relx::query::count(posts.id), "post_count"),
        relx::query::as(relx::query::sum(posts.views), "total_views")
    ).from(departments)
     .join(users, relx::query::on(
         departments.id == users.department_id
     ))
     .join(posts, relx::query::on(
         users.id == posts.user_id
     ))
     .join(comments, relx::query::on(
         posts.id == comments.post_id
     ))
     .where(
         departments.budget > 10000.0 && posts.views >= 100
     )
     .group_by(departments.name)
     .order_by(relx::query::desc(relx::query::count_distinct(users.id)))
     .limit(5);
    
    // Check the SQL generated (expect a complex query)
    EXPECT_EQ(query.to_sql(), "SELECT departments.name, COUNT(DISTINCT users.id) AS user_count, COUNT(posts.id) AS post_count, SUM(posts.views) AS total_views FROM departments JOIN users ON (departments.id = users.department_id) JOIN posts ON (users.id = posts.user_id) JOIN comments ON (posts.id = comments.post_id) WHERE ((departments.budget > ?) AND (posts.views >= ?)) GROUP BY departments.name ORDER BY COUNT(DISTINCT users.id) DESC LIMIT ?");
    
    // Check the parameters - don't check exact string representation for doubles
    auto params = query.bind_params();
    EXPECT_EQ(3, params.size());
    // Just check it starts with 10000 for the double value
    EXPECT_TRUE(params[0].find("10000") == 0) << "Parameter should be approximately 10000, got " << params[0];
    EXPECT_EQ("100", params[1]);
    EXPECT_EQ("5", params[2]);
    
    // Create some sample raw results - order by user_count decreasing
    std::vector<std::string> headers = {"name", "user_count", "post_count", "total_views"};
    std::vector<std::vector<std::string>> rows = {
        {"Engineering", "15", "45", "7500"},
        {"Sales", "12", "36", "6300"},
        {"Marketing", "8", "30", "5200"}
    };
    std::string raw_results = create_raw_results(headers, rows);
    
    // Parse the results
    auto result = relx::result::parse(query, raw_results);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    EXPECT_EQ(3, results.size());
    
    // Check the results - first should be Engineering department
    const auto& first_row = results.at(0);
    
    auto dept_name = first_row.get<std::string>("name");
    ASSERT_TRUE(dept_name) << dept_name.error().message;
    EXPECT_EQ("Engineering", *dept_name);
    
    auto user_count = first_row.get<int>("user_count");
    ASSERT_TRUE(user_count) << user_count.error().message;
    EXPECT_EQ(15, *user_count);
    
    // Define a struct for the aggregated results
    struct DepartmentSummary {
        std::string name;
        int user_count;
        int post_count;
        int total_views;
    };
    
    // Transform the results - verify all rows
    auto summaries = results.transform<DepartmentSummary>([](const relx::result::Row& row) -> relx::result::ResultProcessingResult<DepartmentSummary> {
        auto name = row.get<std::string>("name");
        auto user_count = row.get<int>("user_count");
        auto post_count = row.get<int>("post_count");
        auto total_views = row.get<int>("total_views");
        
        if (!name || !user_count || !post_count || !total_views) {
            return std::unexpected(relx::result::ResultError{"Failed to extract department summary data"});
        }
        
        return DepartmentSummary{*name, *user_count, *post_count, *total_views};
    });
    
    ASSERT_EQ(3, summaries.size());
    
    // Check that results are ordered by user_count (DESC)
    EXPECT_GE(summaries[0].user_count, summaries[1].user_count);
    EXPECT_GE(summaries[1].user_count, summaries[2].user_count);
}

// Test a query with partial column selection
TEST_F(AdvancedQueryTest, PartialColumnSelectionTest) {
    // Create a query that selects only specific columns
    auto query = relx::query::select(
        users.id, users.name
    ).from(users)
     .where(users.age > 25);
    
    // Check the SQL generated - only id and name should be selected
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name FROM users WHERE (users.age > ?)");
    
    // Create some sample raw results
    std::vector<std::string> headers = {"id", "name"};
    std::vector<std::vector<std::string>> rows = {
        {"1", "John Doe"},
        {"2", "Jane Smith"},
        {"3", "Bob Johnson"}
    };
    std::string raw_results = create_raw_results(headers, rows);
    
    // Parse the results
    auto result = relx::result::parse(query, raw_results);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    EXPECT_EQ(3, results.size());
    EXPECT_EQ(2, results.column_names().size());
    
    // Try to access a column not included in the selection (should fail)
    const auto& first_row = results.at(0);
    auto age = first_row.get<int>("age");
    EXPECT_FALSE(age) << "Should not be able to access age column that wasn't selected";
    
    // But we should be able to access the selected columns
    auto id = first_row.get<int>("id");
    ASSERT_TRUE(id) << id.error().message;
    EXPECT_EQ(1, *id);
    
    auto name = first_row.get<std::string>("name");
    ASSERT_TRUE(name) << name.error().message;
    EXPECT_EQ("John Doe", *name);
}

// Test a LEFT JOIN query with potential NULL values
TEST_F(AdvancedQueryTest, LeftJoinWithNullValues) {
    // Create a query that left joins users and posts
    auto query = relx::query::select(
        users.id, users.name, posts.id, posts.title
    ).from(users)
     .left_join(posts, relx::query::on(
         users.id == posts.user_id
     ));
    
    // Check the SQL generated
    EXPECT_EQ(query.to_sql(), "SELECT users.id, users.name, posts.id, posts.title FROM users LEFT JOIN posts ON (users.id = posts.user_id)");
    
    // Create some sample raw results with NULLs for users without posts
    std::vector<std::string> headers = {"id", "name", "id", "title"};
    std::vector<std::vector<std::string>> rows = {
        {"1", "John Doe", "101", "First Post"},
        {"2", "Jane Smith", "NULL", "NULL"},  // User without posts
        {"3", "Bob Johnson", "301", "Bob's Post"}
    };
    std::string raw_results = create_raw_results(headers, rows);
    
    // Parse the results
    auto result = relx::result::parse(query, raw_results);
    ASSERT_TRUE(result) << result.error().message;
    
    const auto& results = *result;
    EXPECT_EQ(3, results.size());
    
    // Check handling of NULL values
    const auto& second_row = results.at(1);
    
    // User fields should have values
    auto user_id = second_row.get<int>(0);
    ASSERT_TRUE(user_id) << user_id.error().message;
    EXPECT_EQ(2, *user_id);
    
    auto user_name = second_row.get<std::string>(1);
    ASSERT_TRUE(user_name) << user_name.error().message;
    EXPECT_EQ("Jane Smith", *user_name);
    
    // Post fields should be NULL
    auto post_id = second_row.get<int>(2);
    EXPECT_FALSE(post_id) << "Post ID should be NULL";
    
    // But post_id as optional<int> should work
    auto opt_post_id = second_row.get<std::optional<int>>(2);
    ASSERT_TRUE(opt_post_id) << opt_post_id.error().message;
    EXPECT_FALSE(*opt_post_id);  // The optional itself is valid, but contains nullopt
    
    // Similar for title
    auto opt_title = second_row.get<std::optional<std::string>>(3);
    ASSERT_TRUE(opt_title) << opt_title.error().message;
    EXPECT_FALSE(*opt_title);
} 