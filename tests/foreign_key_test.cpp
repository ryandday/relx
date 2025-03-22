#include <gtest/gtest.h>
#include <sqllib/schema.hpp>

using namespace sqllib::schema;

// Parent tables
struct User {
    static constexpr auto table_name = "users";
    
    column<"id", int> id;
    column<"username", std::string> username;
    
    primary_key<&User::id> pk;
};

struct Category {
    static constexpr auto table_name = "categories";
    
    column<"id", int> id;
    column<"name", std::string> name_col;
    
    primary_key<&Category::id> pk;
};

// Child table with multiple foreign keys
struct Post {
    static constexpr auto table_name = "posts";
    
    column<"id", int> id;
    column<"title", std::string> title;
    column<"content", std::string> content;
    column<"user_id", int> user_id;
    column<"category_id", std::optional<int>> category_id;
    
    // Primary key
    primary_key<&Post::id> pk;
    
    // Foreign keys with different reference actions
    foreign_key<&Post::user_id, &User::id> user_fk;
    foreign_key<&Post::category_id, &Category::id> category_fk{reference_action::set_null, reference_action::cascade};
};

TEST(ForeignKeyTest, BasicForeignKey) {
    Post post;
    
    // Test the SQL definition of simple foreign key with default actions
    EXPECT_EQ(post.user_fk.sql_definition(), "FOREIGN KEY (user_id) REFERENCES users (id)");
    
    // Test that the foreign key appears in the table constraints
    std::string constraints = collect_constraint_definitions(post);
    EXPECT_TRUE(constraints.find("FOREIGN KEY (user_id) REFERENCES users (id)") != std::string::npos);
    
    // Test that the foreign key appears in the CREATE TABLE statement
    std::string sql = create_table_sql(post);
    EXPECT_TRUE(sql.find("FOREIGN KEY (user_id) REFERENCES users (id)") != std::string::npos);
}

TEST(ForeignKeyTest, ForeignKeyWithActions) {
    Post post;
    
    // Test the SQL definition with custom actions
    EXPECT_EQ(post.category_fk.sql_definition(), 
              "FOREIGN KEY (category_id) REFERENCES categories (id) ON DELETE SET NULL ON UPDATE CASCADE");
    
    // Test that the actions appear in the CREATE TABLE statement
    std::string sql = create_table_sql(post);
    EXPECT_TRUE(sql.find("FOREIGN KEY (category_id) REFERENCES categories (id) ON DELETE SET NULL ON UPDATE CASCADE") 
                != std::string::npos);
}

TEST(ForeignKeyTest, MultipleForeignKeys) {
    Post post;
    
    // Test that both foreign keys appear in the constraints
    std::string constraints = collect_constraint_definitions(post);
    EXPECT_TRUE(constraints.find("FOREIGN KEY (user_id) REFERENCES users (id)") != std::string::npos);
    EXPECT_TRUE(constraints.find("FOREIGN KEY (category_id) REFERENCES categories (id)") != std::string::npos);
    
    // Test that both foreign keys appear in the CREATE TABLE statement
    std::string sql = create_table_sql(post);
    EXPECT_TRUE(sql.find("FOREIGN KEY (user_id) REFERENCES users (id)") != std::string::npos);
    EXPECT_TRUE(sql.find("FOREIGN KEY (category_id) REFERENCES categories (id)") != std::string::npos);
}

TEST(ForeignKeyTest, ReferenceActions) {
    // Test all reference actions
    EXPECT_EQ(std::string_view(reference_action_to_string(reference_action::cascade)), "CASCADE");
    EXPECT_EQ(std::string_view(reference_action_to_string(reference_action::restrict)), "RESTRICT");
    EXPECT_EQ(std::string_view(reference_action_to_string(reference_action::set_null)), "SET NULL");
    EXPECT_EQ(std::string_view(reference_action_to_string(reference_action::set_default)), "SET DEFAULT");
    EXPECT_EQ(std::string_view(reference_action_to_string(reference_action::no_action)), "NO ACTION");
    
    // Test default action
    foreign_key<&Post::user_id, &User::id> default_fk;
    EXPECT_EQ(default_fk.sql_definition(), "FOREIGN KEY (user_id) REFERENCES users (id)");
    
    // Test with custom actions
    foreign_key<&Post::user_id, &User::id> cascade_fk{reference_action::cascade, reference_action::cascade};
    EXPECT_EQ(cascade_fk.sql_definition(), 
              "FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE ON UPDATE CASCADE");
    
    foreign_key<&Post::user_id, &User::id> restrict_fk{reference_action::restrict, reference_action::no_action};
    EXPECT_EQ(restrict_fk.sql_definition(), 
              "FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE RESTRICT");
    
    foreign_key<&Post::user_id, &User::id> set_default_fk{reference_action::no_action, reference_action::set_default};
    EXPECT_EQ(set_default_fk.sql_definition(), 
              "FOREIGN KEY (user_id) REFERENCES users (id) ON UPDATE SET DEFAULT");
} 