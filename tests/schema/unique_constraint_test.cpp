#include <gtest/gtest.h>
#include <relx/schema/column.hpp>
#include <relx/schema/unique_constraint.hpp>
#include <relx/schema/table.hpp>
#include <string>

using namespace relx::schema;

// Test table with unique constraints
struct Employee {
    static constexpr auto table_name = "employees";
    
    column<"id", int> id;
    column<"email", std::string> email;
    column<"first_name", std::string> first_name;
    column<"last_name", std::string> last_name;
    column<"department", std::string> department;
    column<"position", std::string> position;
    
    // Single column unique constraint
    unique_constraint<&Employee::email> unique_email;
    
    // Multi-column unique constraint
    composite_unique_constraint<&Employee::first_name, &Employee::last_name> unique_name;
    
    // Another multi-column unique constraint using the alias 'unique'
    unique<&Employee::department, &Employee::position> unique_dept_pos;
};

TEST(UniqueConstraintTest, SingleColumnUnique) {
    // Test single column unique constraint
    unique_constraint<&Employee::email> email_unique;
    EXPECT_EQ(email_unique.sql_definition(), "UNIQUE (email)");
    
    // Another example
    unique_constraint<&Employee::id> id_unique;
    EXPECT_EQ(id_unique.sql_definition(), "UNIQUE (id)");
}

TEST(UniqueConstraintTest, MultiColumnUnique) {
    // Test multi-column unique constraint
    composite_unique_constraint<&Employee::first_name, &Employee::last_name> name_unique;
    EXPECT_EQ(name_unique.sql_definition(), "UNIQUE (first_name, last_name)");
    
    // Test using the unified 'unique' alias
    unique<&Employee::department, &Employee::position> dept_pos_unique;
    EXPECT_EQ(dept_pos_unique.sql_definition(), "UNIQUE (department, position)");
    
    // Test with three columns
    unique<&Employee::first_name, &Employee::last_name, &Employee::department> name_dept_unique;
    EXPECT_EQ(name_dept_unique.sql_definition(), "UNIQUE (first_name, last_name, department)");
}

TEST(UniqueConstraintTest, TableWithUniqueConstraints) {
    Employee e;
    
    // Generate CREATE TABLE SQL with unique constraints
    std::string create_sql = create_table(e);
    
    // Validate SQL contains unique constraints
    EXPECT_TRUE(create_sql.find("UNIQUE (email)") != std::string::npos);
    EXPECT_TRUE(create_sql.find("UNIQUE (first_name, last_name)") != std::string::npos);
    EXPECT_TRUE(create_sql.find("UNIQUE (department, position)") != std::string::npos);
} 
