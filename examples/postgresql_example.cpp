#include <iostream>
#include <vector>
#include <string>
#include <optional>

#include <sqllib/postgresql.hpp>
#include <sqllib/schema.hpp>
#include <sqllib/query.hpp>
#include <sqllib/results.hpp>

// Define the database schema
// Users table definition
struct Users {
    static constexpr auto table_name = "users";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::column<"age", int> age;
    sqllib::schema::column<"is_active", bool> is_active;
    
    sqllib::schema::primary_key<&Users::id> pk;
    sqllib::schema::unique_constraint<&Users::email> unique_email;
};

// Posts table definition with foreign key to Users
struct Posts {
    static constexpr auto table_name = "posts";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    sqllib::schema::column<"views", int> views;
    sqllib::schema::column<"created_at", std::string> created_at;
    
    sqllib::schema::primary_key<&Posts::id> pk;
    sqllib::schema::foreign_key<&Posts::user_id, &Users::id> user_fk;
};

// Comments table definition with foreign key to Posts
struct Comments {
    static constexpr auto table_name = "comments";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"post_id", int> post_id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"content", std::string> content;
    sqllib::schema::column<"created_at", std::string> created_at;
    
    sqllib::schema::primary_key<&Comments::id> pk;
    sqllib::schema::foreign_key<&Comments::post_id, &Posts::id> post_fk;
    sqllib::schema::foreign_key<&Comments::user_id, &Users::id> user_fk;
};

// Function to handle errors in connection results
template<typename T>
bool check_result(const sqllib::ConnectionResult<T>& result, const std::string& operation) {
    if (!result) {
        std::cerr << "Error during " << operation << ": " << result.error().message << std::endl;
        return false;
    }
    return true;
}

void print_divider() {
    std::cout << "\n" << std::string(80, '-') << "\n" << std::endl;
}

// Function to create tables
bool create_tables(sqllib::Connection& conn) {
    std::cout << "Creating tables..." << std::endl;
    
    // Create Users table
    std::string create_users_sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id SERIAL PRIMARY KEY,
            name TEXT NOT NULL,
            email TEXT NOT NULL UNIQUE,
            age INTEGER NOT NULL,
            is_active BOOLEAN NOT NULL DEFAULT TRUE
        )
    )";
    
    auto users_result = conn.execute_raw(create_users_sql);
    if (!check_result(users_result, "creating users table")) return false;
    
    // Create Posts table
    std::string create_posts_sql = R"(
        CREATE TABLE IF NOT EXISTS posts (
            id SERIAL PRIMARY KEY,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            title TEXT NOT NULL,
            content TEXT NOT NULL,
            views INTEGER NOT NULL DEFAULT 0,
            created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    auto posts_result = conn.execute_raw(create_posts_sql);
    if (!check_result(posts_result, "creating posts table")) return false;
    
    // Create Comments table
    std::string create_comments_sql = R"(
        CREATE TABLE IF NOT EXISTS comments (
            id SERIAL PRIMARY KEY,
            post_id INTEGER NOT NULL REFERENCES posts(id) ON DELETE CASCADE,
            user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            content TEXT NOT NULL,
            created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    auto comments_result = conn.execute_raw(create_comments_sql);
    if (!check_result(comments_result, "creating comments table")) return false;
    
    std::cout << "Tables created successfully!" << std::endl;
    return true;
}

// Function to insert sample data
bool insert_sample_data(sqllib::Connection& conn) {
    std::cout << "Inserting sample data..." << std::endl;
    
    // Begin a transaction
    auto begin_result = conn.begin_transaction();
    if (!check_result(begin_result, "beginning transaction")) return false;
    
    try {
        // Insert users
        auto insert_user1 = conn.execute_raw(
            "INSERT INTO users (name, email, age, is_active) VALUES ($1, $2, $3, $4) RETURNING id",
            {"Alice Johnson", "alice@example.com", "28", "true"}
        );
        if (!check_result(insert_user1, "inserting user 1")) throw std::runtime_error("Failed to insert user 1");
        
        int user1_id = (*insert_user1)[0].get<int>("id").value_or(0);
        
        auto insert_user2 = conn.execute_raw(
            "INSERT INTO users (name, email, age, is_active) VALUES ($1, $2, $3, $4) RETURNING id",
            {"Bob Smith", "bob@example.com", "35", "true"}
        );
        if (!check_result(insert_user2, "inserting user 2")) throw std::runtime_error("Failed to insert user 2");
        
        int user2_id = (*insert_user2)[0].get<int>("id").value_or(0);
        
        auto insert_user3 = conn.execute_raw(
            "INSERT INTO users (name, email, age, is_active) VALUES ($1, $2, $3, $4) RETURNING id",
            {"Charlie Davis", "charlie@example.com", "42", "false"}
        );
        if (!check_result(insert_user3, "inserting user 3")) throw std::runtime_error("Failed to insert user 3");
        
        int user3_id = (*insert_user3)[0].get<int>("id").value_or(0);
        
        // Insert posts
        auto insert_post1 = conn.execute_raw(
            "INSERT INTO posts (user_id, title, content, views) VALUES ($1, $2, $3, $4) RETURNING id",
            {std::to_string(user1_id), "First Post", "This is Alice's first post content", "150"}
        );
        if (!check_result(insert_post1, "inserting post 1")) throw std::runtime_error("Failed to insert post 1");
        
        int post1_id = (*insert_post1)[0].get<int>("id").value_or(0);
        
        auto insert_post2 = conn.execute_raw(
            "INSERT INTO posts (user_id, title, content, views) VALUES ($1, $2, $3, $4) RETURNING id",
            {std::to_string(user2_id), "Hello World", "Bob's introduction post", "75"}
        );
        if (!check_result(insert_post2, "inserting post 2")) throw std::runtime_error("Failed to insert post 2");
        
        int post2_id = (*insert_post2)[0].get<int>("id").value_or(0);
        
        auto insert_post3 = conn.execute_raw(
            "INSERT INTO posts (user_id, title, content, views) VALUES ($1, $2, $3, $4) RETURNING id",
            {std::to_string(user1_id), "Second Post", "Alice's follow-up post", "200"}
        );
        if (!check_result(insert_post3, "inserting post 3")) throw std::runtime_error("Failed to insert post 3");
        
        int post3_id = (*insert_post3)[0].get<int>("id").value_or(0);
        
        // Insert comments
        auto insert_comment1 = conn.execute_raw(
            "INSERT INTO comments (post_id, user_id, content) VALUES ($1, $2, $3)",
            {std::to_string(post1_id), std::to_string(user2_id), "Great first post!"}
        );
        if (!check_result(insert_comment1, "inserting comment 1")) throw std::runtime_error("Failed to insert comment 1");
        
        auto insert_comment2 = conn.execute_raw(
            "INSERT INTO comments (post_id, user_id, content) VALUES ($1, $2, $3)",
            {std::to_string(post1_id), std::to_string(user3_id), "I agree with Bob"}
        );
        if (!check_result(insert_comment2, "inserting comment 2")) throw std::runtime_error("Failed to insert comment 2");
        
        auto insert_comment3 = conn.execute_raw(
            "INSERT INTO comments (post_id, user_id, content) VALUES ($1, $2, $3)",
            {std::to_string(post2_id), std::to_string(user1_id), "Welcome, Bob!"}
        );
        if (!check_result(insert_comment3, "inserting comment 3")) throw std::runtime_error("Failed to insert comment 3");
        
        // Commit the transaction
        auto commit_result = conn.commit_transaction();
        if (!check_result(commit_result, "committing transaction")) throw std::runtime_error("Failed to commit transaction");
        
        std::cout << "Sample data inserted successfully!" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error during data insertion: " << e.what() << std::endl;
        
        // Rollback the transaction on error
        auto rollback_result = conn.rollback_transaction();
        if (!rollback_result) {
            std::cerr << "Error during rollback: " << rollback_result.error().message << std::endl;
        }
        
        return false;
    }
}

// Function to demonstrate basic SQL queries
void demonstrate_basic_queries(sqllib::Connection& conn) {
    print_divider();
    std::cout << "DEMONSTRATING BASIC QUERIES" << std::endl;
    print_divider();
    
    // 1. Basic SELECT query - get all users
    std::cout << "1. Selecting all users:" << std::endl;
    
    Users u;
    auto users_query = sqllib::query::select(u.id, u.name, u.email, u.age, u.is_active)
        .from(u)
        .order_by(u.id);
    
    auto users_result = conn.execute(users_query);
    if (check_result(users_result, "selecting users")) {
        for (const auto& row : *users_result) {
            std::cout << "ID: " << *row.get<int>("id") 
                      << ", Name: " << *row.get<std::string>("name")
                      << ", Email: " << *row.get<std::string>("email")
                      << ", Age: " << *row.get<int>("age")
                      << ", Active: " << (*row.get<bool>("is_active") ? "Yes" : "No")
                      << std::endl;
        }
    }
    
    print_divider();
    
    // 2. SELECT with WHERE condition - get active users over 30
    std::cout << "2. Selecting active users over 30:" << std::endl;
    
    auto active_users_query = sqllib::query::select(u.id, u.name, u.age)
        .from(u)
        .where((u.age > 30) && (u.is_active == true));
    
    auto active_users_result = conn.execute(active_users_query);
    if (check_result(active_users_result, "selecting active users over 30")) {
        for (const auto& row : *active_users_result) {
            std::cout << "ID: " << *row.get<int>("id") 
                      << ", Name: " << *row.get<std::string>("name")
                      << ", Age: " << *row.get<int>("age")
                      << std::endl;
        }
    }
    
    print_divider();
    
    // 3. UPDATE query - update a user's active status
    std::cout << "3. Updating user's active status:" << std::endl;
    
    auto update_query = sqllib::query::update(u)
        .set(u.is_active, true)
        .where(u.name == "Charlie Davis");
    
    auto update_result = conn.execute(update_query);
    if (check_result(update_result, "updating user")) {
        std::cout << "Updated Charlie's active status to true" << std::endl;
    }
    
    // Verify the update
    auto verify_update = sqllib::query::select(u.id, u.name, u.is_active)
        .from(u)
        .where(u.name == "Charlie Davis");
    
    auto verify_result = conn.execute(verify_update);
    if (check_result(verify_result, "verifying update")) {
        const auto& row = (*verify_result)[0];
        std::cout << "Verified: " << *row.get<std::string>("name") 
                  << "'s active status is now: " << (*row.get<bool>("is_active") ? "true" : "false")
                  << std::endl;
    }
    
    print_divider();
    
    // 4. DELETE query - delete a non-existent user (safe example)
    std::cout << "4. Deleting a user (safe example):" << std::endl;
    
    auto delete_query = sqllib::query::delete_from(u)
        .where(u.name == "NonExistentUser");
    
    auto delete_result = conn.execute(delete_query);
    if (check_result(delete_result, "deleting user")) {
        std::cout << "Delete query executed successfully (0 rows affected)" << std::endl;
    }
}

// Function to demonstrate complex queries
void demonstrate_complex_queries(sqllib::Connection& conn) {
    print_divider();
    std::cout << "DEMONSTRATING COMPLEX QUERIES" << std::endl;
    print_divider();
    
    Users u;
    Posts p;
    Comments c;
    
    // 1. JOIN query - Get posts with their author information
    std::cout << "1. JOIN: Posts with author information:" << std::endl;
    
    auto join_query = sqllib::query::select(p.id, p.title, p.views, u.name.as("author_name"), u.email.as("author_email"))
        .from(p)
        .join(u, sqllib::query::on(p.user_id == u.id))
        .order_by(p.views.desc());
    
    auto join_result = conn.execute(join_query);
    if (check_result(join_result, "post-author join")) {
        for (const auto& row : *join_result) {
            std::cout << "Post ID: " << *row.get<int>("id") 
                      << ", Title: " << *row.get<std::string>("title")
                      << ", Views: " << *row.get<int>("views")
                      << ", Author: " << *row.get<std::string>("author_name")
                      << " (" << *row.get<std::string>("author_email") << ")"
                      << std::endl;
        }
    }
    
    print_divider();
    
    // 2. Aggregate functions - Count posts per user with total views
    std::cout << "2. Aggregates: Post counts and total views per user:" << std::endl;
    
    auto agg_query = sqllib::query::select_expr(
        u.name,
        sqllib::query::as(sqllib::query::count(p.id), "post_count"),
        sqllib::query::as(sqllib::query::sum(p.views), "total_views")
    )
    .from(u)
    .left_join(p, sqllib::query::on(u.id == p.user_id))
    .group_by(u.id, u.name)
    .order_by(sqllib::query::desc(sqllib::query::sum(p.views)));
    
    auto agg_result = conn.execute(agg_query);
    if (check_result(agg_result, "aggregate query")) {
        for (const auto& row : *agg_result) {
            std::cout << "User: " << *row.get<std::string>("name") 
                      << ", Post Count: " << *row.get<int>("post_count")
                      << ", Total Views: " << *row.get<int>("total_views")
                      << std::endl;
        }
    }
    
    print_divider();
    
    // 3. Complex JOIN with subquery and HAVING - Get popular posts with comment counts
    std::cout << "3. Complex JOIN: Popular posts with comment counts:" << std::endl;
    
    auto complex_query = sqllib::query::select_expr(
        p.id,
        p.title,
        u.name.as("author"),
        p.views,
        sqllib::query::as(sqllib::query::count(c.id), "comment_count")
    )
    .from(p)
    .join(u, sqllib::query::on(p.user_id == u.id))
    .left_join(c, sqllib::query::on(p.id == c.post_id))
    .group_by(p.id, p.title, u.name, p.views)
    .having(
        (p.views > 50) &&
        (sqllib::query::count(c.id) > 0)
    )
    .order_by(p.views.desc());
    
    auto complex_result = conn.execute(complex_query);
    if (check_result(complex_result, "complex join query")) {
        for (const auto& row : *complex_result) {
            std::cout << "Post ID: " << *row.get<int>("id") 
                      << ", Title: " << *row.get<std::string>("title")
                      << ", Author: " << *row.get<std::string>("author")
                      << ", Views: " << *row.get<int>("views")
                      << ", Comments: " << *row.get<int>("comment_count")
                      << std::endl;
        }
    }
    
    print_divider();
    
    // 4. Advanced query with CASE expression - User activity categories
    std::cout << "4. Advanced CASE expression: User activity categories:" << std::endl;
    
    auto case_query = sqllib::query::select_expr(
        u.name,
        sqllib::query::as(
            sqllib::query::case_()
                .when(
                    sqllib::query::count(p.id) == 0,
                    "Inactive"
                )
                .when(
                    (sqllib::query::count(p.id) >= 1) && (sqllib::query::count(p.id) < 3),
                    "Casual"
                )
                .else_(
                    "Power User"
                )
                .build(),
            "user_category"
        ),
        sqllib::query::as(sqllib::query::count(p.id), "post_count")
    )
    .from(u)
    .left_join(p, sqllib::query::on(u.id == p.user_id))
    .group_by(u.id, u.name)
    .order_by(sqllib::query::count(p.id).desc());
    
    auto case_result = conn.execute(case_query);
    if (check_result(case_result, "case expression query")) {
        for (const auto& row : *case_result) {
            std::cout << "User: " << *row.get<std::string>("name") 
                      << ", Category: " << *row.get<std::string>("user_category")
                      << ", Post Count: " << *row.get<int>("post_count")
                      << std::endl;
        }
    }
}

// Function to clean up data
bool clean_up(sqllib::Connection& conn) {
    print_divider();
    std::cout << "Cleaning up database..." << std::endl;
    
    std::vector<std::string> drop_tables = {
        "DROP TABLE IF EXISTS comments",
        "DROP TABLE IF EXISTS posts",
        "DROP TABLE IF EXISTS users"
    };
    
    bool success = true;
    for (const auto& sql : drop_tables) {
        auto result = conn.execute_raw(sql);
        if (!result) {
            std::cerr << "Error dropping table: " << result.error().message << std::endl;
            success = false;
        }
    }
    
    if (success) {
        std::cout << "Database cleaned up successfully!" << std::endl;
    }
    
    return success;
}

int main() {
    // Connection string - Modify with your PostgreSQL connection details
    std::string conn_string = "host=localhost port=5435 dbname=sqllib_example user=postgres password=postgres";
    
    // Create a connection
    sqllib::PostgreSQLConnection conn(conn_string);
    
    // Connect to the database
    auto connect_result = conn.connect();
    if (!check_result(connect_result, "connecting to database")) {
        return 1;
    }
    
    std::cout << "Connected to PostgreSQL database successfully!" << std::endl;
    
    // Create tables
    if (!create_tables(conn)) {
        conn.disconnect();
        return 1;
    }
    
    // Insert sample data
    if (!insert_sample_data(conn)) {
        clean_up(conn);
        conn.disconnect();
        return 1;
    }
    
    // Demonstrate basic queries
    demonstrate_basic_queries(conn);
    
    // Demonstrate complex queries
    demonstrate_complex_queries(conn);
    
    // Clean up
    clean_up(conn);
    
    // Disconnect
    auto disconnect_result = conn.disconnect();
    if (!check_result(disconnect_result, "disconnecting from database")) {
        return 1;
    }
    
    std::cout << "Disconnected from PostgreSQL database successfully!" << std::endl;
    
    return 0;
} 