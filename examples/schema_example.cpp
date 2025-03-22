#include <iostream>
#include <sqllib/schema.hpp>

using namespace sqllib::schema;
// Example of a table definition that satisfies TableConcept
struct Users {
    // Static table name required by TableConcept
    static constexpr auto table_name = "users";
    
    // Column definitions
    column<"id", int> id;
    column<"name", std::string> username;
    column<"email", std::string> email;
    column<"age", int> age;
    
    // Constraints
    primary_key<&Users::id> pk;
    sqllib::schema::index<&Users::email> email_idx{index_type::unique};
};

// Example of a table with nullable columns
struct Posts {
    // Static table name required by TableConcept
    static constexpr auto table_name = "posts";
    
    // Column definitions
    column<"id", int> id;
    column<"title", std::string> title;
    column<"content", std::string> content;
    column<"user_id", std::optional<int>> user_id;
    
    // Constraints
    primary_key<&Posts::id> pk;
    foreign_key<&Posts::user_id, &Users::id> user_fk;
};

int main() {
    // Create instances of tables
    Users users;
    Posts posts;
    
    // Generate and print CREATE TABLE statements
    std::cout << "Users Table SQL:\n" << create_table_sql(users) << "\n\n";
    std::cout << "Posts Table SQL:\n" << create_table_sql(posts) << std::endl;
    
    // Generate and print index creation statements
    std::cout << "\nIndex creation statements:\n";
    std::cout << users.email_idx.create_index_sql() << std::endl;
    
    return 0;
}
