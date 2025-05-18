#include <iostream>
#include <vector>
#include <string>
#include <optional>

#include <relx/postgresql.hpp>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/results.hpp>

// Define the database schema
// Users table definition
struct Users {
    static constexpr auto table_name = "users";
    
    relx::column<Users, "id", int, relx::identity<>> id;
    relx::column<Users, "name", std::string> name;
    relx::column<Users, "email", std::string> email;
    relx::column<Users, "age", int> age;
    relx::column<Users, "is_active", bool> is_active;
    
    relx::table_primary_key<&Users::id> pk;
    relx::unique_constraint<&Users::email> unique_email;
};

// Posts table definition with foreign key to Users
struct Posts {
    static constexpr auto table_name = "posts";
    
    relx::column<Posts, "id", int, relx::identity<>> id;
    relx::column<Posts, "user_id", int> user_id;
    relx::column<Posts, "title", std::string> title;
    relx::column<Posts, "content", std::string> content;
    relx::column<Posts, "views", int> views;
    relx::column<Posts, "created_at", std::string> created_at;
    
    relx::table_primary_key<&Posts::id> pk;
    relx::foreign_key<&Posts::user_id, &Users::id> user_fk;
};

// Comments table definition with foreign key to Posts
struct Comments {
    static constexpr auto table_name = "comments";
    
    relx::column<Comments, "id", int, relx::identity<>> id;
    relx::column<Comments, "post_id", int> post_id;
    relx::column<Comments, "user_id", int> user_id;
    relx::column<Comments, "content", std::string> content;
    relx::column<Comments, "created_at", std::string> created_at;
    
    relx::table_primary_key<&Comments::id> pk;
    relx::foreign_key<&Comments::post_id, &Posts::id> post_fk;
    relx::foreign_key<&Comments::user_id, &Users::id> user_fk;
};

// Function to handle errors in connection results
template<typename T>
bool check_result(const relx::ConnectionResult<T>& result, const std::string& operation) {
    if (!result) {
        std::print("Error during {}: {} ({})", operation, result.error().message, result.error().error_code);
        return false;
    }
    return true;
}

void print_divider() {
    std::println("\n{}\n", std::string(80, '-'));
}

// Function to create tables
bool create_tables(relx::Connection& conn) {
    std::println("Creating tables...");
    
    Users users;
    // Create Users table
    auto create_users_sql = relx::create_table(users);
    auto users_result = conn.execute(create_users_sql);
    if (!check_result(users_result, "creating users table")) return false;
    
    // Create Posts table
    Posts posts;
    auto create_posts_sql = relx::schema::create_table(posts);
    auto posts_result = conn.execute(create_posts_sql);
    if (!check_result(posts_result, "creating posts table")) return false;
    // Create Comments table
    Comments comments;
    auto create_comments_sql = relx::create_table(comments);
    
    auto comments_result = conn.execute(create_comments_sql);
    if (!check_result(comments_result, "creating comments table")) return false;
    
    std::println("Tables created successfully!");
    return true;
}

bool drop_tables(relx::Connection& conn) {
    std::println("Dropping tables...");
    
    Users users;
    Posts posts;
    Comments comments;

    auto drop_comments = relx::schema::drop_table(comments);
    auto drop_posts = relx::schema::drop_table(posts);
    auto drop_users = relx::schema::drop_table(users);
    
    auto drop_comments_result = conn.execute(drop_comments);
    if (!check_result(drop_comments_result, "dropping comments table")) return false;

    auto drop_posts_result = conn.execute(drop_posts);
    if (!check_result(drop_posts_result, "dropping posts table")) return false;

    auto drop_users_result = conn.execute(drop_users);
    if (!check_result(drop_users_result, "dropping users table")) return false;
    
    std::println("Tables dropped successfully!");
    return true;
}

// Function to insert sample data
bool insert_sample_data(relx::Connection& conn) {
    std::println("Inserting sample data...");
    
    // Begin a transaction
    auto begin_result = conn.begin_transaction();
    if (!check_result(begin_result, "beginning transaction")) return false;
    
    try {
        // Insert users
        Users users;
        auto insert_user1 = relx::insert_into(users).columns(
            users.name, users.email, users.age, users.is_active
        ).values(
            "Alice Johnson", "alice@example.com", "28", "true"
        ).returning(users.id);
        auto insert_user1_result = conn.execute(insert_user1);
        if (!check_result(insert_user1_result, "inserting user 1")) throw std::runtime_error("Failed to insert user 1");
        int user1_id = (*insert_user1_result).at(0).get<int>("id").value_or(0);
        
        auto insert_user2 = relx::insert_into(users).columns(
            users.name, users.email, users.age, users.is_active
        ).values(
            "Bob Smith", "bob@example.com", "35", "true"
        ).returning(users.id);
        auto insert_user2_result = conn.execute(insert_user2);
        if (!check_result(insert_user2_result, "inserting user 2")) throw std::runtime_error("Failed to insert user 2");
        
        int user2_id = (*insert_user2_result)[0].get<int>("id").value_or(0);
        
        auto insert_user3 = relx::insert_into(users).columns(
            users.name, users.email, users.age, users.is_active
        ).values(
            "Charlie Davis", "charlie@example.com", "42", "false"
        ).returning(users.id);
        auto insert_user3_result = conn.execute(insert_user3);
        if (!check_result(insert_user3_result, "inserting user 3")) throw std::runtime_error("Failed to insert user 3");
        
        int user3_id = (*insert_user3_result)[0].get<int>("id").value_or(0);
        // Insert posts
        Posts posts;
        auto insert_post1 = relx::insert_into(posts).columns(
            posts.user_id, posts.title, posts.content, posts.views, posts.created_at
        ).values(
            user1_id, "First Post", "This is Alice's first post content", "150", "2024-04-29 12:00:00"
        ).returning(posts.id);
        auto insert_post1_result = conn.execute(insert_post1);
        if (!check_result(insert_post1_result, "inserting post 1")) throw std::runtime_error("Failed to insert post 1");
        
        int post1_id = (*insert_post1_result)[0].get<int>("id").value_or(0);
        auto insert_post2 = relx::insert_into(posts).columns(
            posts.user_id, posts.title, posts.content, posts.views, posts.created_at
        ).values(
            user2_id, "Hello World", "Bob's introduction post", "75", "2024-04-29 13:00:00"
        ).returning(posts.id);
        auto insert_post2_result = conn.execute(insert_post2);
        if (!check_result(insert_post2_result, "inserting post 2")) throw std::runtime_error("Failed to insert post 2");
        
        int post2_id = (*insert_post2_result)[0].get<int>("id").value_or(0);
        auto insert_post3 = relx::insert_into(posts).columns(
            posts.user_id, posts.title, posts.content, posts.views, posts.created_at
        ).values(
            user1_id, "Second Post", "Alice's follow-up post", "200", "2024-04-29 14:00:00"
        ).returning(posts.id);
        auto insert_post3_result = conn.execute(insert_post3);
        if (!check_result(insert_post3_result, "inserting post 3")) throw std::runtime_error("Failed to insert post 3");
        
        int post3_id = (*insert_post3_result)[0].get<int>("id").value_or(0);
        // Insert comments
        Comments comments;
        auto insert_comment1 = relx::insert_into(comments).columns(
            comments.post_id, comments.user_id, comments.content, comments.created_at
        ).values(
            post1_id, user2_id, "Great first post!", "2024-04-29 12:30:00"
        ).returning(comments.id);
        auto insert_comment1_result = conn.execute(insert_comment1);
        if (!check_result(insert_comment1_result, "inserting comment 1")) throw std::runtime_error("Failed to insert comment 1");
        
        auto insert_comment2 = relx::insert_into(comments).columns(
            comments.post_id, comments.user_id, comments.content, comments.created_at
        ).values(
            post1_id, user3_id, "I agree with Bob", "2024-04-29 12:45:00"
        ).returning(comments.id);
        auto insert_comment2_result = conn.execute(insert_comment2);
        if (!check_result(insert_comment2_result, "inserting comment 2")) throw std::runtime_error("Failed to insert comment 2");
        
        auto insert_comment3 = relx::insert_into(comments).columns(
            comments.post_id, comments.user_id, comments.content, comments.created_at
        ).values(
            post2_id, user1_id, "Welcome, Bob!", "2024-04-29 13:15:00"
        ).returning(comments.id);
        auto insert_comment3_result = conn.execute(insert_comment3);
        if (!check_result(insert_comment3_result, "inserting comment 3")) throw std::runtime_error("Failed to insert comment 3");
        
        // Commit the transaction
        auto commit_result = conn.commit_transaction();
        if (!check_result(commit_result, "committing transaction")) throw std::runtime_error("Failed to commit transaction");
        
        std::println("Sample data inserted successfully!");
        return true;
    }
    catch (const std::exception& e) {
        std::print("Error during data insertion: {}", e.what());
        
        // Rollback the transaction on error
        auto rollback_result = conn.rollback_transaction();
        if (!rollback_result) {
            std::print("Error during rollback: {}", rollback_result.error().message);
        }
        
        return false;
    }
}

// Function to demonstrate basic SQL queries
void demonstrate_basic_queries(relx::Connection& conn) {
    print_divider();
    std::println("DEMONSTRATING BASIC QUERIES");
    print_divider();
    
    // 1. Basic SELECT query - get all users
    std::println("1. Selecting all users:");
    
    Users u;
    auto users_query = relx::select(u.id, u.name, u.email, u.age, u.is_active)
        .from(u)
        .order_by(u.id);
    
    auto users_result = conn.execute(users_query);
    if (check_result(users_result, "selecting users")) {
        for (const auto& row : *users_result) {
            std::println("ID: {}, Name: {}, Email: {}, Age: {}, Active: {}", 
                        *row.get<int>("id"), 
                        *row.get<std::string>("name"),
                        *row.get<std::string>("email"),
                        *row.get<int>("age"),
                        (*row.get<bool>("is_active") ? "Yes" : "No"));
        }
    }
    
    print_divider();
    
    // 2. SELECT with WHERE condition - get active users over 30
    std::println("2. Selecting active users over 30:");
    
    auto active_users_query = relx::select(u.id, u.name, u.age)
        .from(u)
        .where((u.age > 30) && (u.is_active == true));
    
    auto active_users_result = conn.execute(active_users_query);
    if (check_result(active_users_result, "selecting active users over 30")) {
        for (const auto& row : *active_users_result) {
            std::println("ID: {}, Name: {}, Age: {}", 
                        *row.get<int>("id"), 
                        *row.get<std::string>("name"),
                        *row.get<int>("age"));
        }
    }
    
    print_divider();
    
    // 3. UPDATE query - update a user's active status
    std::println("3. Updating user's active status:");
    
    auto update_query = relx::update(u)
        .set(u.is_active, true)
        .where(u.name == "Charlie Davis");
    
    auto update_result = conn.execute(update_query);
    if (check_result(update_result, "updating user")) {
        std::println("Updated Charlie's active status to true");
    }
    
    // Verify the update
    auto verify_update = relx::select(u.id, u.name, u.is_active)
        .from(u)
        .where(u.name == "Charlie Davis");
    
    auto verify_result = conn.execute(verify_update);
    if (check_result(verify_result, "verifying update")) {
        const auto& row = (*verify_result)[0];
        std::println("Verified: {}'s active status is now: {}", 
                    *row.get<std::string>("name"),
                    (*row.get<bool>("is_active") ? "true" : "false"));
    }
    
    print_divider();
    
    // 4. DELETE query - delete a non-existent user (safe example)
    std::println("4. Deleting a user (safe example):");
    
    auto delete_query = relx::delete_from(u)
        .where(u.name == "NonExistentUser");
    
    auto delete_result = conn.execute(delete_query);
    if (check_result(delete_result, "deleting user")) {
        std::println("Delete query executed successfully (0 rows affected)");
    }
}

// Function to demonstrate complex queries
void demonstrate_complex_queries(relx::Connection& conn) {
    print_divider();
    std::println("DEMONSTRATING COMPLEX QUERIES");
    print_divider();
    
    Users u;
    Posts p;
    Comments c;
    
    // 1. JOIN query - Get posts with their author information
    std::println("1. JOIN: Posts with author information:");
    
    auto join_query = relx::select(p.id, p.title, p.views, relx::as(u.name, "author_name"), relx::as(u.email, "author_email"))
        .from(p)
        .join(u, relx::on(p.user_id == u.id))
        .order_by(relx::desc(p.views));
    
    auto join_result = conn.execute(join_query);
    if (check_result(join_result, "post-author join")) {
        for (const auto& row : *join_result) {
            std::println("Post ID: {}, Title: {}, Views: {}, Author: {} ({})", 
                        *row.get<int>("id"), 
                        *row.get<std::string>("title"),
                        *row.get<int>("views"),
                        *row.get<std::string>("author_name"),
                        *row.get<std::string>("author_email"));
        }
    }
    
    print_divider();
    
    // 2. Aggregate functions - Count posts per user with total views
    std::println("2. Aggregates: Post counts and total views per user:");
    
    auto agg_query = relx::select_expr(
        u.name,
        relx::as(relx::count(p.id), "post_count"),
        relx::as(relx::sum(p.views), "total_views")
    )
    .from(u)
    .left_join(p, relx::on(u.id == p.user_id))
    .group_by(u.id, u.name)
    .order_by(relx::desc(relx::sum(p.views)));
    
    auto agg_result = conn.execute(agg_query);
    if (check_result(agg_result, "aggregate query")) {
        for (const auto& row : *agg_result) {
            std::println("User: {}, Post Count: {}, Total Views: {}", 
                        *row.get<std::string>("name"),
                        *row.get<int>("post_count"),
                        *row.get<int>("total_views"));
        }
    }
    
    print_divider();
    
    // 3. Complex JOIN with subquery and HAVING - Get popular posts with comment counts
    std::println("3. Complex JOIN: Popular posts with comment counts:");
    
    auto complex_query = relx::select_expr(
        p.id,
        p.title,
        relx::as(u.name, "author"),
        p.views,
        relx::as(relx::count(c.id), "comment_count")
    )
    .from(p)
    .join(u, relx::on(p.user_id == u.id))
    .left_join(c, relx::on(p.id == c.post_id))
    .group_by(p.id, p.title, u.name, p.views)
    .having(
        (p.views > 50) &&
        (relx::count(c.id) > 0)
    )
    .order_by(relx::desc(p.views));
    
    auto complex_result = conn.execute(complex_query);
    if (check_result(complex_result, "complex join query")) {
        for (const auto& row : *complex_result) {
            std::println("Post ID: {}, Title: {}, Author: {}, Views: {}, Comments: {}", 
                        *row.get<int>("id"), 
                        *row.get<std::string>("title"),
                        *row.get<std::string>("author"),
                        *row.get<int>("views"),
                        *row.get<int>("comment_count"));
        }
    }
    
    print_divider();
    
    // 4. Advanced query with CASE expression - User activity categories
    std::println("4. Advanced CASE expression: User activity categories:");
    
    auto case_query = relx::select_expr(
        u.name,
        relx::as(
            relx::case_()
                .when(
                    relx::count(p.id) == 0,
                    "Inactive"
                )
                .when(
                    (relx::count(p.id) >= 1) && (relx::count(p.id) < 3),
                    "Casual"
                )
                .else_(
                    "Power User"
                )
                .build(),
            "user_category"
        ),
        relx::as(relx::count(p.id), "post_count")
    )
    .from(u)
    .left_join(p, relx::on(u.id == p.user_id))
    .group_by(u.id, u.name)
    .order_by(relx::desc(relx::count(p.id)));
    
    auto case_result = conn.execute(case_query);
    if (check_result(case_result, "case expression query")) {
        for (const auto& row : *case_result) {
            std::println("User: {}, Category: {}, Post Count: {}", 
                        *row.get<std::string>("name"),
                        *row.get<std::string>("user_category"),
                        *row.get<int>("post_count"));
        }
    }
}

// Function to clean up data
bool clean_up(relx::Connection& conn) {
    print_divider();
    std::println("Cleaning up database...");
    
    Users users;
    Posts posts;
    Comments comments;

    auto drop_comments = relx::schema::drop_table(comments);
    auto drop_posts = relx::schema::drop_table(posts);
    auto drop_users = relx::schema::drop_table(users);
    
    bool success = true;
    auto drop_comments_result = conn.execute(drop_comments);
    if (!check_result(drop_comments_result, "dropping comments table")) return false;

    auto drop_posts_result = conn.execute(drop_posts);
    if (!check_result(drop_posts_result, "dropping posts table")) return false;

    auto drop_users_result = conn.execute(drop_users);
    if (!check_result(drop_users_result, "dropping users table")) return false;
    
    std::println("Database cleaned up successfully!");
    
    return true;
}

int main() {
    // Connection string - Modify with your PostgreSQL connection details
    std::string conn_string = "host=localhost port=5435 dbname=relx_example user=postgres password=postgres";
    
    // Create a connection
    relx::PostgreSQLConnection conn(conn_string);
    
    // Connect to the database
    auto connect_result = conn.connect();
    if (!check_result(connect_result, "connecting to database")) {
        return 1;
    }
    
    std::println("Connected to PostgreSQL database successfully!");
    
    // drop tables if they exist
    if (!drop_tables(conn)) {
        conn.disconnect();
        return 1;
    }

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
    
    std::println("Disconnected from PostgreSQL database successfully!");
    
    return 0;
} 
