#include <iostream>
#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>
#include <string>
#include <vector>

// Define a simple schema
struct User {
    static constexpr auto table_name = "users";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::column<"age", int> age;
    sqllib::schema::column<"active", bool> active;
    
    sqllib::schema::primary_key<&User::id> pk;
    sqllib::schema::unique_constraint<&User::email> unique_email;
};

int main() {
    // Create an instance of the User table
    User users;
    
    // Example 1: Basic DELETE query - delete all users
    auto delete_all = sqllib::query::delete_from(users);
    std::cout << "Example 1: " << delete_all.to_sql() << std::endl;
    
    // Example 2: DELETE with a WHERE clause - delete a specific user by ID
    auto delete_by_id = sqllib::query::delete_from(users)
        .where(sqllib::query::to_expr(users.id) == sqllib::query::val(1));
    std::cout << "Example 2: " << delete_by_id.to_sql() << std::endl;
    
    // Output parameters (for prepared statements)
    auto params2 = delete_by_id.bind_params();
    std::cout << "   Parameters: " << params2[0] << std::endl;
    
    // Example 3: DELETE with complex conditions - delete inactive adult users
    auto delete_inactive_adults = sqllib::query::delete_from(users)
        .where(sqllib::query::to_expr(users.active) == sqllib::query::val(false) && 
               sqllib::query::to_expr(users.age) >= sqllib::query::val(18));
    std::cout << "Example 3: " << delete_inactive_adults.to_sql() << std::endl;
    
    // Output parameters
    auto params3 = delete_inactive_adults.bind_params();
    std::cout << "   Parameters: " << params3[0] << ", " << params3[1] << std::endl;
    
    // Example 4: DELETE with IN condition - delete users with specific IDs
    std::vector<std::string> ids = {"1", "3", "5"};
    auto delete_by_ids = sqllib::query::delete_from(users)
        .where_in(users.id, ids);
    std::cout << "Example 4: " << delete_by_ids.to_sql() << std::endl;
    
    // Output parameters
    auto params4 = delete_by_ids.bind_params();
    std::cout << "   Parameters: ";
    for (const auto& param : params4) {
        std::cout << param << " ";
    }
    std::cout << std::endl;
    
    // Example 5: DELETE with LIKE condition - delete users with gmail email
    auto delete_gmail = sqllib::query::delete_from(users)
        .where(sqllib::query::like(sqllib::query::to_expr(users.email), "%@gmail.com"));
    std::cout << "Example 5: " << delete_gmail.to_sql() << std::endl;
    
    // Output parameters
    auto params5 = delete_gmail.bind_params();
    std::cout << "   Parameters: " << params5[0] << std::endl;
    
    // Example 6: DELETE with safety mechanism for deleting all records
    auto delete_all_safe = sqllib::query::delete_from(users)
        .where(sqllib::query::val(true) == sqllib::query::val(true)); // Always true condition
    std::cout << "Example 6: " << delete_all_safe.to_sql() << std::endl;
    
    return 0;
} 