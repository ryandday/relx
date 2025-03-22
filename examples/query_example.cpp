#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <optional>

// Define example tables
struct Users {
    static constexpr auto table_name = "users";
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::column<"age", int> age;
    sqllib::schema::column<"bio", std::optional<std::string>> bio;
    
    sqllib::schema::primary_key<&Users::id> pk;
    sqllib::schema::unique_constraint<&Users::email> unique_email;
};

struct posts {
    static constexpr auto table_name = "posts";
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    sqllib::schema::column<"created_at", std::string> created_at;
    
    sqllib::schema::primary_key<&posts::id> pk;
    sqllib::schema::foreign_key<&posts::user_id, &Users::id> user_fk;
};

// Utility function to print SQL and parameters
void print_sql_and_params(const auto& query, const std::string& description) {
    std::cout << "\n=== " << description << " ===\n";
    std::cout << "SQL: " << query.to_sql() << "\n";

    auto params = query.bind_params();
    if (params.empty()) {
        std::cout << "Parameters: None\n";
    } else {
        std::cout << "Parameters: [";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << "\"" << params[i] << "\"";
        }
        std::cout << "]\n";
    }
}

int main() {
    std::cout << "SQLlib Query Builder Example\n";
    std::cout << "============================\n";

    // Create tables
    Users u;
    posts p;

    // Example 1: Simple SELECT query
    auto query1 = sqllib::query::select(u.id, u.name, u.email)
        .from(u);
    print_sql_and_params(query1, "Simple SELECT");

    // Example 2: SELECT with WHERE condition
    auto query2 = sqllib::query::select(u.id, u.name)
        .from(u)
        .where(sqllib::query::to_expr(u.age) > sqllib::query::val(18));
    print_sql_and_params(query2, "SELECT with WHERE");

    // Example 3: SELECT with JOIN
    auto query3 = sqllib::query::select(u.name, p.title)
        .from(u)
        .join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)));
    print_sql_and_params(query3, "SELECT with JOIN");

    // Example 4: SELECT with complex conditions
    auto query4 = sqllib::query::select(u.id, u.name, u.email)
        .from(u)
        .where(
            (sqllib::query::to_expr(u.age) >= sqllib::query::val(18)) && 
            (sqllib::query::to_expr(u.name) != sqllib::query::val("")) &&
            (sqllib::query::like(sqllib::query::to_expr(u.email), "%@example.com"))
        );
    print_sql_and_params(query4, "SELECT with complex WHERE");

    // Example 5: SELECT with aggregate functions
    auto query5 = sqllib::query::select_expr(
        sqllib::query::as(sqllib::query::count_all(), "user_count"),
        sqllib::query::as(sqllib::query::avg(sqllib::query::to_expr(u.age)), "average_age")
    )
    .from(u)
    .where(sqllib::query::to_expr(u.age) > sqllib::query::val(21));
    print_sql_and_params(query5, "SELECT with aggregates");

    // Example 6: SELECT with GROUP BY and HAVING
    auto query6 = sqllib::query::select(u.id,
        sqllib::query::as(sqllib::query::count(sqllib::query::to_expr(p.id)), "post_count"))
        .from(u)
        .join(p, sqllib::query::on(sqllib::query::to_expr(u.id) == sqllib::query::to_expr(p.user_id)))
        .group_by(sqllib::query::to_expr(u.id))
        .having(sqllib::query::count(sqllib::query::to_expr(p.id)) > sqllib::query::val(5));
    print_sql_and_params(query6, "SELECT with GROUP BY and HAVING");

    // Example 7: SELECT with CASE expression
    auto query7 = sqllib::query::select(u.name, 
        sqllib::query::as(
            sqllib::query::case_()
                .when(sqllib::query::to_expr(u.age) < sqllib::query::val(18), sqllib::query::val("Minor"))
                .when(sqllib::query::to_expr(u.age) < sqllib::query::val(65), sqllib::query::val("Adult"))
                .else_(sqllib::query::val("Senior"))
                .build(),
            "age_group"
        ))
        .from(u);
    print_sql_and_params(query7, "SELECT with CASE expression");

    // Example 8: SELECT with IN condition
    std::vector<std::string> names = {"Alice", "Bob", "Charlie"};
    auto query8 = sqllib::query::select(u.id, u.email)
        .from(u)
        .where(sqllib::query::in(sqllib::query::to_expr(u.name), names));
    print_sql_and_params(query8, "SELECT with IN condition");

    return 0;
} 