/**
 * SQLlib Schema Tutorial
 * ======================
 * 
 * This example demonstrates how to use the sqllib schema system to define
 * database tables with type safety and generate SQL. We'll start with basic
 * concepts and progressively explore more advanced features.
 */

#include <iostream>
#include <sqllib/schema.hpp>

/////////////////////////////////////////////////////////////////////////////
// BASIC TABLE DEFINITION
/////////////////////////////////////////////////////////////////////////////

/**
 * The simplest way to define a table is to create a struct with:
 * 1. A static table_name constant
 * 2. One or more column members
 * 
 * Each column is defined using the column template with these parameters:
 * - Column name as a string literal
 * - C++ type for the column data
 */
struct SimpleUsers {
    // Required: Static table name (satisfies TableConcept)
    static constexpr auto table_name = "simple_users";
    
    // Basic column definitions
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
};

/////////////////////////////////////////////////////////////////////////////
// NULLABLE COLUMNS
/////////////////////////////////////////////////////////////////////////////

/**
 * SQLlib supports nullable columns via std::optional.
 * A column defined with std::optional<T> will be nullable in the database.
 */
struct UsersWithNullable {
    static constexpr auto table_name = "users_with_nullable";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    
    // This column can be NULL in the database
    sqllib::schema::column<"bio", std::optional<std::string>> bio;
};

/////////////////////////////////////////////////////////////////////////////
// PRIMARY KEYS
/////////////////////////////////////////////////////////////////////////////

/**
 * Primary keys are defined using the primary_key template,
 * which takes a pointer to a column member as its template parameter.
 */
struct UsersWithPrimaryKey {
    static constexpr auto table_name = "users_with_pk";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    
    // Define a primary key constraint on the id column
    sqllib::schema::primary_key<&UsersWithPrimaryKey::id> pk;
};

/////////////////////////////////////////////////////////////////////////////
// INDEXES
/////////////////////////////////////////////////////////////////////////////

/**
 * Indexes can be defined on columns to improve query performance.
 * The index template takes a pointer to a column member.
 * Different index types are supported through the index_type enum.
 */
struct UsersWithIndexes {
    static constexpr auto table_name = "users_with_indexes";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    sqllib::schema::column<"email", std::string> email;
    
    // Primary key
    sqllib::schema::primary_key<&UsersWithIndexes::id> pk;
    
    // Regular index on username
    sqllib::schema::index<&UsersWithIndexes::username> username_idx;
    
    // Unique index on email
    sqllib::schema::index<&UsersWithIndexes::email> email_idx{sqllib::schema::index_type::unique};
};

/////////////////////////////////////////////////////////////////////////////
// DEFAULT VALUES
/////////////////////////////////////////////////////////////////////////////

/**
 * Columns can have default values using the DefaultValue template.
 * The template parameter is the default value, which is automatically 
 * converted to the appropriate SQL representation.
 */
struct UsersWithDefaults {
    static constexpr auto table_name = "users_with_defaults";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    
    // Integer default value
    sqllib::schema::column<"age", int, sqllib::schema::DefaultValue<25>> age;
    
    // Boolean default value
    sqllib::schema::column<"is_admin", bool, sqllib::schema::DefaultValue<false>> is_admin;
    
    // Default value for nullable column - explicit NULL
    sqllib::schema::column<"notes", std::optional<std::string>, sqllib::schema::null_default> notes;
};

/////////////////////////////////////////////////////////////////////////////
// FOREIGN KEYS
/////////////////////////////////////////////////////////////////////////////

/**
 * Foreign keys establish relationships between tables.
 * The foreign_key template takes two parameters:
 * 1. Pointer to the column in the current table (the foreign key)
 * 2. Pointer to the referenced column in another table
 */
struct Posts {
    static constexpr auto table_name = "posts";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    sqllib::schema::column<"user_id", int> user_id;
    
    // Primary key
    sqllib::schema::primary_key<&Posts::id> pk;
    
    // Foreign key referencing UsersWithPrimaryKey
    sqllib::schema::foreign_key<&Posts::user_id, &UsersWithPrimaryKey::id> user_fk;
};

/////////////////////////////////////////////////////////////////////////////
// UNIQUE CONSTRAINTS
/////////////////////////////////////////////////////////////////////////////

/**
 * Unique constraints ensure that column values are unique across the table.
 * They can be defined on single columns or multiple columns (composite).
 */
struct UniqueConstraintDemo {
    static constexpr auto table_name = "unique_constraints_demo";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::column<"first_name", std::string> first_name;
    sqllib::schema::column<"last_name", std::string> last_name;
    
    // Primary key
    sqllib::schema::primary_key<&UniqueConstraintDemo::id> pk;
    
    // Single column unique constraint
    sqllib::schema::unique_constraint<&UniqueConstraintDemo::email> unique_email;
    
    // Composite unique constraint on first_name + last_name
    sqllib::schema::composite_unique_constraint<
        &UniqueConstraintDemo::first_name,
        &UniqueConstraintDemo::last_name
    > unique_name;
};

/////////////////////////////////////////////////////////////////////////////
// CHECK CONSTRAINTS
/////////////////////////////////////////////////////////////////////////////

/**
 * Check constraints enforce business rules at the database level.
 * There are two types:
 * 1. General table-level constraints
 * 2. Column-specific constraints
 * 
 */
struct CheckConstraintDemo {
    static constexpr auto table_name = "check_constraints_demo";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"age", int> age;
    sqllib::schema::column<"salary", double> salary;
    sqllib::schema::column<"status", std::string> status;
    
    // Primary key
    sqllib::schema::primary_key<&CheckConstraintDemo::id> pk;
    
    // Column-level check constraints
    sqllib::schema::column_check_constraint<&CheckConstraintDemo::age, "age >= 18"> age_check;
    sqllib::schema::column_check_constraint<&CheckConstraintDemo::salary, "salary > 0"> salary_check;
    
    // Table-level check constraint (not associated with any specific column)
    sqllib::schema::check_constraint<"status IN ('active', 'inactive', 'pending')"> status_check;
};

/////////////////////////////////////////////////////////////////////////////
// COMPREHENSIVE EXAMPLE
/////////////////////////////////////////////////////////////////////////////

/**
 * Now let's put it all together in a more realistic database schema
 * for a blog application with multiple related tables.
 */

// Users table
struct Users {
    static constexpr auto table_name = "users";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"username", std::string> username;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::column<"password_hash", std::string> password_hash;
    sqllib::schema::column<"bio", std::optional<std::string>> bio;
    sqllib::schema::column<"is_active", bool, sqllib::schema::DefaultValue<true>> is_active;
    
    // Constraints
    sqllib::schema::primary_key<&Users::id> pk;
    sqllib::schema::unique_constraint<&Users::username> unique_username;
    sqllib::schema::unique_constraint<&Users::email> unique_email;
};

// Blog posts table
struct BlogPosts {
    static constexpr auto table_name = "blog_posts";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    sqllib::schema::column<"published", bool, sqllib::schema::DefaultValue<false>> published;
    
    // Constraints
    sqllib::schema::primary_key<&BlogPosts::id> pk;
    sqllib::schema::foreign_key<&BlogPosts::user_id, &Users::id> user_fk;
};

// Comments table
struct Comments {
    static constexpr auto table_name = "comments";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"post_id", int> post_id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"content", std::string> content;
    
    // Constraints
    sqllib::schema::primary_key<&Comments::id> pk;
    sqllib::schema::foreign_key<&Comments::post_id, &BlogPosts::id> post_fk;
    sqllib::schema::foreign_key<&Comments::user_id, &Users::id> user_fk;
};

// Tags table for categorizing posts
struct Tags {
    static constexpr auto table_name = "tags";
    
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    
    sqllib::schema::primary_key<&Tags::id> pk;
    sqllib::schema::unique_constraint<&Tags::name> unique_name;
};

// Junction table for many-to-many relationship between posts and tags
struct PostTags {
    static constexpr auto table_name = "post_tags";
    
    sqllib::schema::column<"post_id", int> post_id;
    sqllib::schema::column<"tag_id", int> tag_id;
    
    // Composite primary key
    sqllib::schema::composite_primary_key<&PostTags::post_id, &PostTags::tag_id> pk;
    
    // Foreign keys
    sqllib::schema::foreign_key<&PostTags::post_id, &BlogPosts::id> post_fk;
    sqllib::schema::foreign_key<&PostTags::tag_id, &Tags::id> tag_fk;
};

int main() {
    // Create instances of our tables
    SimpleUsers simple_users;
    UsersWithNullable users_with_nullable;
    UsersWithPrimaryKey users_with_pk;
    UsersWithIndexes users_with_indexes;
    UsersWithDefaults users_with_defaults;
    Posts posts;
    UniqueConstraintDemo unique_demo;
    CheckConstraintDemo check_demo;
    
    // The blog application tables
    Users users;
    BlogPosts blog_posts;
    Comments comments;
    Tags tags;
    PostTags post_tags;
    
    // Display SQL for basic tables
    std::cout << "BASIC TABLE DEFINITION\n";
    std::cout << "======================\n";
    std::cout << sqllib::schema::create_table_sql(simple_users) << "\n\n";
    
    std::cout << "NULLABLE COLUMNS\n";
    std::cout << "===============\n";
    std::cout << sqllib::schema::create_table_sql(users_with_nullable) << "\n\n";
    
    std::cout << "PRIMARY KEYS\n";
    std::cout << "============\n";
    std::cout << sqllib::schema::create_table_sql(users_with_pk) << "\n\n";
    
    std::cout << "INDEXES\n";
    std::cout << "=======\n";
    std::cout << sqllib::schema::create_table_sql(users_with_indexes) << "\n";
    // Secondary indices are typically created after the table is created, which is why
    // we're calling create_index_sql() here instead of in the table definition.
    std::cout << users_with_indexes.username_idx.create_index_sql() << "\n";
    std::cout << users_with_indexes.email_idx.create_index_sql() << "\n\n";
    
    std::cout << "DEFAULT VALUES\n";
    std::cout << "==============\n";
    std::cout << sqllib::schema::create_table_sql(users_with_defaults) << "\n\n";
    
    std::cout << "FOREIGN KEYS\n";
    std::cout << "============\n";
    std::cout << sqllib::schema::create_table_sql(posts) << "\n\n";
    
    std::cout << "UNIQUE CONSTRAINTS\n";
    std::cout << "=================\n";
    std::cout << sqllib::schema::create_table_sql(unique_demo) << "\n\n";
    
    std::cout << "CHECK CONSTRAINTS\n";
    std::cout << "=================\n";
    std::cout << sqllib::schema::create_table_sql(check_demo) << "\n\n";
    
    // Display SQL for the blog application
    std::cout << "BLOG APPLICATION SCHEMA\n";
    std::cout << "======================\n";
    std::cout << "Users Table:\n" << sqllib::schema::create_table_sql(users) << "\n\n";
    std::cout << "Blog Posts Table:\n" << sqllib::schema::create_table_sql(blog_posts) << "\n\n";
    std::cout << "Comments Table:\n" << sqllib::schema::create_table_sql(comments) << "\n\n";
    std::cout << "Tags Table:\n" << sqllib::schema::create_table_sql(tags) << "\n\n";
    std::cout << "Post-Tags Junction Table:\n" << sqllib::schema::create_table_sql(post_tags) << "\n\n";
    
    return 0;
}
