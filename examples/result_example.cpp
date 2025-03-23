#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>
#include <sqllib/results.hpp>
#include <iostream>
#include <string>
#include <optional>
#include <iomanip>
#include <vector>
#include <tuple>

// Define table schemas
struct Users {
    static constexpr auto table_name = "users";
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    
    sqllib::schema::primary_key<&Users::id> pk;
};

struct Posts {
    static constexpr auto table_name = "posts";
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"user_id", int> user_id;
    
    sqllib::schema::primary_key<&Posts::id> pk;
    sqllib::schema::foreign_key<&Posts::user_id, &Users::id> user_fk;
};

// Custom structure to hold joined data
struct UserPost {
    int user_id;
    std::string user_name;
    int post_id;
    std::string post_title;
};

// Simulate database query execution
std::string execute_query(const std::string& query) {
    // In a real implementation, this would connect to a database and execute the query
    // For this example, we'll just return some fake results formatted as CSV
    
    // Pretend these are the column headers from the database
    std::string result = "user_id|user_name|post_id|post_title\n";
    
    // Add some fake data rows
    result += "1|John Doe|101|First Post\n";
    result += "1|John Doe|102|Second Post\n";
    result += "2|Jane Smith|201|Hello World\n";
    
    return result;
}

// Pretty print a UserPost object
void print_user_post(const UserPost& up) {
    std::cout << "User: " << up.user_id << " (" << up.user_name << ") - ";
    std::cout << "Post: " << up.post_id << " \"" << up.post_title << "\"" << std::endl;
}

int main() {
    // Create table instances
    Users u;
    Posts p;
    
    // Define the column variables
    auto user_id = sqllib::query::to_expr(u.id);
    auto user_name = sqllib::query::to_expr(u.name);
    auto post_id = sqllib::query::to_expr(p.id);
    auto post_title = sqllib::query::to_expr(p.title);
    
    // Build a query to join Users and Posts
    auto query = sqllib::query::select(user_id, user_name, post_id, post_title)
        .from(u)
        .join(p, sqllib::query::on(user_id == sqllib::query::to_expr(p.user_id)));
    
    // In a real implementation, this would be passed to a database connection
    std::cout << "Query: " << query.to_sql() << std::endl;
    
    // Simulate executing the query and getting results
    std::string raw_results = execute_query(query.to_sql());
    
    // Parse the raw results into a structured format using a delimiter
    auto results_or_error = sqllib::result::parse(query, raw_results);
    if (!results_or_error) {
        std::cerr << "Error parsing results: " << results_or_error.error().message << std::endl;
        return 1;
    }
    
    const auto& results = *results_or_error;
    
    std::cout << "\nFound " << results.size() << " rows with columns: ";
    for (const auto& column : results.column_names()) {
        std::cout << column << " ";
    }
    std::cout << std::endl;
    
    // Method 1: Basic iteration
    std::cout << "\nMethod 1: Basic iteration" << std::endl;
    for (const auto& row : results) {
        auto user_id = row.get<int>("user_id");
        auto user_name = row.get<std::string>("user_name");
        auto post_id = row.get<int>("post_id");
        auto post_title = row.get<std::string>("post_title");
        
        if (user_id && user_name && post_id && post_title) {
            std::cout << "User: " << *user_id << " (";
            std::cout << *user_name << ") - ";
            std::cout << "Post: " << *post_id << " \"";
            std::cout << *post_title << "\"" << std::endl;
        }
    }
    
    // Method 2: Using structured binding with as
    std::cout << "\nMethod 2: Using structured binding" << std::endl;
    for (const auto [user_id, user_name, post_id, post_title] : 
            results.as<int, std::string, int, std::string>()) {
        std::cout << "User: " << user_id << " (" << user_name << ") - ";
        std::cout << "Post: " << post_id << " \"" << post_title << "\"" << std::endl;
    }
    
    // Method 3: Transform to custom objects
    std::cout << "\nMethod 3: Transform to custom objects" << std::endl;
    auto user_posts = results.transform<UserPost>([](const auto& row) -> sqllib::result::ResultProcessingResult<UserPost> {
        // Inside lambda with auto parameter, 'row' is dependent so we need 'template'
        auto user_id = row.template get<int>("user_id");
        auto user_name = row.template get<std::string>("user_name");
        auto post_id = row.template get<int>("post_id");
        auto post_title = row.template get<std::string>("post_title");
        
        if (!user_id || !user_name || !post_id || !post_title) {
            return std::unexpected(sqllib::result::ResultError{"Failed to extract user post data"});
        }
        
        return UserPost{*user_id, *user_name, *post_id, *post_title};
    });
    
    for (const auto& up : user_posts) {
        print_user_post(up);
    }
    
    return 0;
} 