#include <sqllib/schema.hpp>
#include <iostream>
#include <string>

// Define our tables
namespace {

// User table
struct user : sqllib::schema::table<"users"> {
    // Define columns
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"name", std::string> name;
    sqllib::schema::column<"email", std::string> email;
    sqllib::schema::nullable_column<"bio", std::string> bio;
    
    // Define constraints
    sqllib::schema::primary_key<&user::id> pk;
    sqllib::schema::index<&user::email> email_idx;
    
    // Implement the column definitions collection
    std::string collect_column_definitions() const override {
        std::string result;
        result += "    " + id.sql_definition();
        result += ",\n    " + name.sql_definition();
        result += ",\n    " + email.sql_definition();
        result += ",\n    " + bio.sql_definition();
        return result;
    }
    
    // Implement the constraint definitions collection
    std::string collect_constraint_definitions() const override {
        return "    " + pk.sql_definition();
    }
};

// Post table
struct post : sqllib::schema::table<"posts"> {
    // Define columns
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"title", std::string> title;
    sqllib::schema::column<"content", std::string> content;
    
    // Define constraints
    sqllib::schema::primary_key<&post::id> pk;
    sqllib::schema::foreign_key<&post::user_id, &user::id> user_fk;
    
    // Implement the column definitions collection
    std::string collect_column_definitions() const override {
        std::string result;
        result += "    " + id.sql_definition();
        result += ",\n    " + user_id.sql_definition();
        result += ",\n    " + title.sql_definition();
        result += ",\n    " + content.sql_definition();
        return result;
    }
    
    // Implement the constraint definitions collection
    std::string collect_constraint_definitions() const override {
        return "    " + pk.sql_definition() + ",\n    " + user_fk.sql_definition();
    }
};

// Comment table
struct comment : sqllib::schema::table<"comments"> {
    // Define columns
    sqllib::schema::column<"id", int> id;
    sqllib::schema::column<"post_id", int> post_id;
    sqllib::schema::column<"user_id", int> user_id;
    sqllib::schema::column<"content", std::string> content;
    
    // Define constraints
    sqllib::schema::primary_key<&comment::id> pk;
    sqllib::schema::foreign_key<&comment::post_id, &post::id> post_fk;
    sqllib::schema::foreign_key<&comment::user_id, &user::id> user_fk;
    
    // Implement the column definitions collection
    std::string collect_column_definitions() const override {
        std::string result;
        result += "    " + id.sql_definition();
        result += ",\n    " + post_id.sql_definition();
        result += ",\n    " + user_id.sql_definition();
        result += ",\n    " + content.sql_definition();
        return result;
    }
    
    // Implement the constraint definitions collection
    std::string collect_constraint_definitions() const override {
        return "    " + pk.sql_definition() + 
               ",\n    " + post_fk.sql_definition() +
               ",\n    " + user_fk.sql_definition();
    }
};

} // anonymous namespace

int main() {
    // Create table instances
    user u;
    post p;
    comment c;
    
    // Print SQL to create tables
    std::cout << "Creating Users table:\n";
    std::cout << u.create_table_sql() << "\n\n";
    
    std::cout << "Creating Posts table:\n";
    std::cout << p.create_table_sql() << "\n\n";
    
    std::cout << "Creating Comments table:\n";
    std::cout << c.create_table_sql() << "\n\n";
    
    // Print index creation SQL
    std::cout << "Creating Indexes:\n";
    std::cout << u.email_idx.create_index_sql() << "\n";
    
    return 0;
}
