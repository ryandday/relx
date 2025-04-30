#include <gtest/gtest.h>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/connection.hpp>
#include <relx/postgresql.hpp>
#include <string>
#include <memory>
#include <optional>
#include <vector>

// Import shared schema definitions
#include "schema_definitions.hpp"

// Test fixture for DML integration tests
class DmlIntegrationTest : public ::testing::Test {
protected:
    using Connection = relx::connection::PostgreSQLConnection;
    
    std::unique_ptr<Connection> conn;
    
    // Schema instances
    schema::Category category;
    schema::Product product;
    schema::Customer customer;
    schema::Order order;
    
    void SetUp() override {
        // Connect to the database
        conn = std::make_unique<Connection>("host=localhost port=5432 dbname=relx_test user=postgres password=postgres");
        auto result = conn->connect();
        ASSERT_TRUE(result) << "Failed to connect: " << result.error().message;
        
        // Clean up any existing tables
        cleanup_database();
        
        // Set up the schema
        setup_schema();
    }
    
    void TearDown() override {
        if (conn && conn->is_connected()) {
            cleanup_database();
            conn->disconnect();
        }
    }
    
    void cleanup_database() {
        auto result = conn->execute_raw("DROP TABLE IF EXISTS orders CASCADE");
        ASSERT_TRUE(result) << "Failed to drop orders table: " << result.error().message;
        
        result = conn->execute_raw("DROP TABLE IF EXISTS inventory CASCADE");
        ASSERT_TRUE(result) << "Failed to drop inventory table: " << result.error().message;
        
        result = conn->execute_raw("DROP TABLE IF EXISTS customers CASCADE");
        ASSERT_TRUE(result) << "Failed to drop customers table: " << result.error().message;
        
        result = conn->execute_raw("DROP TABLE IF EXISTS products CASCADE");
        ASSERT_TRUE(result) << "Failed to drop products table: " << result.error().message;
        
        result = conn->execute_raw("DROP TABLE IF EXISTS categories CASCADE");
        ASSERT_TRUE(result) << "Failed to drop categories table: " << result.error().message;
    }
    
    void setup_schema() {
        // Create tables in dependency order
        auto result = relx::schema::create_table(conn.get(), category);
        ASSERT_TRUE(result) << "Failed to create category table: " << result.error().message;
        
        result = relx::schema::create_table(conn.get(), product);
        ASSERT_TRUE(result) << "Failed to create product table: " << result.error().message;
        
        result = relx::schema::create_table(conn.get(), customer);
        ASSERT_TRUE(result) << "Failed to create customer table: " << result.error().message;
        
        result = relx::schema::create_table(conn.get(), order);
        ASSERT_TRUE(result) << "Failed to create order table: " << result.error().message;
    }
};

// Test INSERT operations
TEST_F(DmlIntegrationTest, InsertOperations) {
    using namespace relx::query;
    
    // Basic single row insert
    auto insert_category = insert_into(category)
        .columns(category.id, category.name, category.description)
        .values(1, "Electronics", "Electronic devices and accessories");
        
    auto result = conn->execute_raw(insert_category.to_sql(), insert_category.bind_params());
    ASSERT_TRUE(result) << "Failed to insert single category: " << result.error().message;
    
    // Verify the insert worked
    auto select_category = select(category.id, category.name, category.description)
        .from(category)
        .where(category.id == 1);
        
    result = conn->execute_raw(select_category.to_sql(), select_category.bind_params());
    ASSERT_TRUE(result) << "Failed to select category: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(1, rows.size()) << "Expected 1 category";
    EXPECT_EQ(1, *rows[0].get<int>(0));
    EXPECT_EQ("Electronics", *rows[0].get<std::string>(1));
    EXPECT_EQ("Electronic devices and accessories", *rows[0].get<std::string>(2));
    
    // Multi-row insert
    auto insert_more_categories = insert_into(category)
        .columns(category.id, category.name, category.description)
        .values(2, "Clothing", "Apparel and fashion items")
        .values(3, "Books", "Books and publications");
        
    result = conn->execute_raw(insert_more_categories.to_sql(), insert_more_categories.bind_params());
    ASSERT_TRUE(result) << "Failed to insert multiple categories: " << result.error().message;
    
    // Verify multi-row insert
    auto select_all_categories = select(category.id, category.name)
        .from(category)
        .order_by(category.id);
        
    result = conn->execute_raw(select_all_categories.to_sql(), select_all_categories.bind_params());
    ASSERT_TRUE(result) << "Failed to select all categories: " << result.error().message;
    
    auto& all_rows = *result;
    ASSERT_EQ(3, all_rows.size()) << "Expected 3 categories after multi-row insert";
    
    // Insert with NULL values
    auto insert_with_null = insert_into(category)
        .columns(category.id, category.name, category.description)
        .values(4, "Misc", std::nullopt);
        
    result = conn->execute_raw(insert_with_null.to_sql(), insert_with_null.bind_params());
    ASSERT_TRUE(result) << "Failed to insert with NULL value: " << result.error().message;
    
    // Verify NULL value insert
    auto select_with_null = select(category.id, category.name, category.description)
        .from(category)
        .where(category.id == 4);
        
    result = conn->execute_raw(select_with_null.to_sql(), select_with_null.bind_params());
    ASSERT_TRUE(result) << "Failed to select category with NULL: " << result.error().message;
    
    auto& null_rows = *result;
    ASSERT_EQ(1, null_rows.size());
    auto desc = null_rows[0].get<std::optional<std::string>>(2);
    EXPECT_FALSE(desc.has_value()) << "Expected NULL description";
    
    // Insert into a table with foreign key constraints
    auto insert_product = insert_into(product)
        .columns(product.id, product.category_id, product.name, product.price, product.sku)
        .values(1, 1, "Smartphone", 999.99, "ELEC001");
        
    result = conn->execute_raw(insert_product.to_sql(), insert_product.bind_params());
    ASSERT_TRUE(result) << "Failed to insert product: " << result.error().message;
    
    // Insert with returning clause
    auto insert_with_returning = insert_into(customer)
        .columns(customer.id, customer.name, customer.email)
        .values(1, "John Doe", "john@example.com")
        .returning(customer.id, customer.name);
        
    result = conn->execute_raw(insert_with_returning.to_sql(), insert_with_returning.bind_params());
    ASSERT_TRUE(result) << "Failed to insert with RETURNING: " << result.error().message;
    
    auto& returning_rows = *result;
    ASSERT_EQ(1, returning_rows.size()) << "Expected 1 row from RETURNING clause";
    EXPECT_EQ(1, *returning_rows[0].get<int>(0));
    EXPECT_EQ("John Doe", *returning_rows[0].get<std::string>(1));
}

// Test UPDATE operations
TEST_F(DmlIntegrationTest, UpdateOperations) {
    using namespace relx::query;
    
    // Insert initial data
    auto insert_categories = insert_into(category)
        .columns(category.id, category.name, category.description)
        .values(1, "Electronics", "Electronic devices")
        .values(2, "Clothing", "Apparel items")
        .values(3, "Books", "Books and publications");
        
    auto result = conn->execute_raw(insert_categories.to_sql(), insert_categories.bind_params());
    ASSERT_TRUE(result) << "Failed to insert categories: " << result.error().message;
    
    // Basic update of a single row
    auto update_single = update(category)
        .set(category.description, "Electronic devices and accessories")
        .where(category.id == 1);
        
    result = conn->execute_raw(update_single.to_sql(), update_single.bind_params());
    ASSERT_TRUE(result) << "Failed to update single category: " << result.error().message;
    
    // Verify update
    auto select_updated = select(category.description)
        .from(category)
        .where(category.id == 1);
        
    result = conn->execute_raw(select_updated.to_sql(), select_updated.bind_params());
    ASSERT_TRUE(result) << "Failed to select updated category: " << result.error().message;
    
    auto& updated_rows = *result;
    ASSERT_EQ(1, updated_rows.size());
    EXPECT_EQ("Electronic devices and accessories", *updated_rows[0].get<std::string>(0));
    
    // Update multiple rows
    auto update_multiple = update(category)
        .set(category.name, category.name + val(" Department"))
        .where(category.id <= 2);
        
    result = conn->execute_raw(update_multiple.to_sql(), update_multiple.bind_params());
    ASSERT_TRUE(result) << "Failed to update multiple categories: " << result.error().message;
    
    // Verify multiple updates
    auto select_multiple = select(category.id, category.name)
        .from(category)
        .where(category.id <= 2)
        .order_by(category.id);
        
    result = conn->execute_raw(select_multiple.to_sql(), select_multiple.bind_params());
    ASSERT_TRUE(result) << "Failed to select multiple updated categories: " << result.error().message;
    
    auto& multiple_rows = *result;
    ASSERT_EQ(2, multiple_rows.size());
    EXPECT_EQ("Electronics Department", *multiple_rows[0].get<std::string>(1));
    EXPECT_EQ("Clothing Department", *multiple_rows[1].get<std::string>(1));
    
    // Update with NULL
    auto update_to_null = update(category)
        .set(category.description, std::nullopt)
        .where(category.id == 3);
        
    result = conn->execute_raw(update_to_null.to_sql(), update_to_null.bind_params());
    ASSERT_TRUE(result) << "Failed to update to NULL: " << result.error().message;
    
    // Verify NULL update
    auto select_null = select(category.description)
        .from(category)
        .where(category.id == 3);
        
    result = conn->execute_raw(select_null.to_sql(), select_null.bind_params());
    ASSERT_TRUE(result) << "Failed to select NULL updated category: " << result.error().message;
    
    auto& null_rows = *result;
    ASSERT_EQ(1, null_rows.size());
    auto desc = null_rows[0].get<std::optional<std::string>>(0);
    EXPECT_FALSE(desc.has_value()) << "Expected NULL description after update";
    
    // Update with subquery
    auto insert_products = insert_into(product)
        .columns(product.id, product.category_id, product.name, product.price, product.sku)
        .values(1, 1, "Smartphone", 999.99, "ELEC001")
        .values(2, 1, "Laptop", 1299.99, "ELEC002");
        
    result = conn->execute_raw(insert_products.to_sql(), insert_products.bind_params());
    ASSERT_TRUE(result) << "Failed to insert products: " << result.error().message;
    
    // Update category description based on product info
    auto update_with_subquery = update(category)
        .set(category.description, val("Category with highest priced item: ") + 
             select_expr(max(product.price))
             .from(product)
             .where(product.category_id == category.id))
        .where(category.id == 1);
             
    result = conn->execute_raw(update_with_subquery.to_sql(), update_with_subquery.bind_params());
    ASSERT_TRUE(result) << "Failed to update with subquery: " << result.error().message;
    
    // Verify subquery update
    auto select_subquery_result = select(category.description)
        .from(category)
        .where(category.id == 1);
        
    result = conn->execute_raw(select_subquery_result.to_sql(), select_subquery_result.bind_params());
    ASSERT_TRUE(result) << "Failed to select subquery updated category: " << result.error().message;
    
    auto& subquery_rows = *result;
    ASSERT_EQ(1, subquery_rows.size());
    
    auto updated_desc = *subquery_rows[0].get<std::string>(0);
    EXPECT_EQ("Category with highest priced item: 1299.99", updated_desc);
    
    // Update with returning clause
    auto update_with_returning = update(category)
        .set(category.name, "Updated Electronics")
        .where(category.id == 1)
        .returning(category.id, category.name);
        
    result = conn->execute_raw(update_with_returning.to_sql(), update_with_returning.bind_params());
    ASSERT_TRUE(result) << "Failed to update with RETURNING: " << result.error().message;
    
    auto& returning_rows = *result;
    ASSERT_EQ(1, returning_rows.size()) << "Expected 1 row from UPDATE RETURNING";
    EXPECT_EQ(1, *returning_rows[0].get<int>(0));
    EXPECT_EQ("Updated Electronics", *returning_rows[0].get<std::string>(1));
}

// Test DELETE operations
TEST_F(DmlIntegrationTest, DeleteOperations) {
    using namespace relx::query;
    
    // Insert initial data
    auto insert_categories = insert_into(category)
        .columns(category.id, category.name, category.description)
        .values(1, "Electronics", "Electronic devices")
        .values(2, "Clothing", "Apparel items")
        .values(3, "Books", "Books and publications")
        .values(4, "Temporary", "Will be deleted");
        
    auto result = conn->execute_raw(insert_categories.to_sql(), insert_categories.bind_params());
    ASSERT_TRUE(result) << "Failed to insert categories: " << result.error().message;
    
    // Delete a single row
    auto delete_single = delete_from(category)
        .where(category.id == 4);
        
    result = conn->execute_raw(delete_single.to_sql(), delete_single.bind_params());
    ASSERT_TRUE(result) << "Failed to delete single category: " << result.error().message;
    
    // Verify single deletion
    auto select_all = select(count_all())
        .from(category);
        
    result = conn->execute_raw(select_all.to_sql(), select_all.bind_params());
    ASSERT_TRUE(result) << "Failed to count categories after delete: " << result.error().message;
    
    auto& count_rows = *result;
    ASSERT_EQ(1, count_rows.size());
    EXPECT_EQ(3, *count_rows[0].get<int>(0)) << "Expected 3 categories after single delete";
    
    // Make sure the right row was deleted
    auto check_deleted = select(category.id)
        .from(category)
        .where(category.id == 4);
        
    result = conn->execute_raw(check_deleted.to_sql(), check_deleted.bind_params());
    ASSERT_TRUE(result) << "Failed to check deleted category: " << result.error().message;
    
    auto& deleted_rows = *result;
    EXPECT_EQ(0, deleted_rows.size()) << "Category with id 4 should be deleted";
    
    // Delete with subquery condition
    // Insert products for categories
    auto insert_products = insert_into(product)
        .columns(product.id, product.category_id, product.name, product.price, product.sku)
        .values(1, 1, "Smartphone", 999.99, "ELEC001")
        .values(2, 2, "T-Shirt", 19.99, "CLTH001");
        
    result = conn->execute_raw(insert_products.to_sql(), insert_products.bind_params());
    ASSERT_TRUE(result) << "Failed to insert products: " << result.error().message;
    
    // Delete categories with products priced over 500
    auto delete_with_subquery = delete_from(category)
        .where(exists(
            select(product.id)
            .from(product)
            .where(product.category_id == category.id && product.price > 500)
        ));
        
    result = conn->execute_raw(delete_with_subquery.to_sql(), delete_with_subquery.bind_params());
    ASSERT_TRUE(result) << "Failed to delete with subquery: " << result.error().message;
    
    // Verify subquery deletion
    result = conn->execute_raw(select_all.to_sql(), select_all.bind_params());
    ASSERT_TRUE(result) << "Failed to count categories after subquery delete: " << result.error().message;
    
    auto& subquery_count = *result;
    ASSERT_EQ(1, subquery_count.size());
    EXPECT_EQ(2, *subquery_count[0].get<int>(0)) << "Expected 2 categories after subquery delete";
    
    // Verify the right category was deleted (Electronics with expensive smartphone)
    auto remaining_categories = select(category.id, category.name)
        .from(category)
        .order_by(category.id);
        
    result = conn->execute_raw(remaining_categories.to_sql(), remaining_categories.bind_params());
    ASSERT_TRUE(result) << "Failed to select remaining categories: " << result.error().message;
    
    auto& remaining_rows = *result;
    ASSERT_EQ(2, remaining_rows.size());
    EXPECT_EQ(2, *remaining_rows[0].get<int>(0)) << "Category 2 (Clothing) should remain";
    EXPECT_EQ(3, *remaining_rows[1].get<int>(0)) << "Category 3 (Books) should remain";
    
    // Delete with returning
    auto delete_with_returning = delete_from(category)
        .where(category.id == 3)
        .returning(category.id, category.name);
        
    result = conn->execute_raw(delete_with_returning.to_sql(), delete_with_returning.bind_params());
    ASSERT_TRUE(result) << "Failed to delete with RETURNING: " << result.error().message;
    
    auto& returning_rows = *result;
    ASSERT_EQ(1, returning_rows.size()) << "Expected 1 row from DELETE RETURNING";
    EXPECT_EQ(3, *returning_rows[0].get<int>(0));
    EXPECT_EQ("Books", *returning_rows[0].get<std::string>(1));
    
    // Verify after returning delete
    result = conn->execute_raw(select_all.to_sql(), select_all.bind_params());
    ASSERT_TRUE(result) << "Failed to count categories after returning delete: " << result.error().message;
    
    auto& final_count = *result;
    ASSERT_EQ(1, final_count.size());
    EXPECT_EQ(1, *final_count[0].get<int>(0)) << "Expected 1 category after all deletes";
}

// Test transaction support
TEST_F(DmlIntegrationTest, TransactionSupport) {
    using namespace relx::query;
    
    // Start a transaction
    auto result = conn->begin_transaction();
    ASSERT_TRUE(result) << "Failed to begin transaction: " << result.error().message;
    
    // Insert within transaction
    auto insert = insert_into(category)
        .columns(category.id, category.name)
        .values(1, "Electronics")
        .values(2, "Clothing");
        
    result = conn->execute_raw(insert.to_sql(), insert.bind_params());
    ASSERT_TRUE(result) << "Failed to insert in transaction: " << result.error().message;
    
    // Verify data is visible within the transaction
    auto select = select(count_all()).from(category);
    result = conn->execute_raw(select.to_sql(), select.bind_params());
    ASSERT_TRUE(result) << "Failed to select in transaction: " << result.error().message;
    
    auto& rows = *result;
    EXPECT_EQ(2, *rows[0].get<int>(0)) << "Expected 2 categories in transaction";
    
    // Rollback the transaction
    result = conn->rollback_transaction();
    ASSERT_TRUE(result) << "Failed to rollback transaction: " << result.error().message;
    
    // Verify data is not visible after rollback
    result = conn->execute_raw(select.to_sql(), select.bind_params());
    ASSERT_TRUE(result) << "Failed to select after rollback: " << result.error().message;
    
    auto& after_rollback = *result;
    EXPECT_EQ(0, *after_rollback[0].get<int>(0)) << "Expected 0 categories after rollback";
    
    // Start another transaction
    result = conn->begin_transaction();
    ASSERT_TRUE(result) << "Failed to begin second transaction: " << result.error().message;
    
    // Insert again
    result = conn->execute_raw(insert.to_sql(), insert.bind_params());
    ASSERT_TRUE(result) << "Failed to insert in second transaction: " << result.error().message;
    
    // Commit the transaction
    result = conn->commit_transaction();
    ASSERT_TRUE(result) << "Failed to commit transaction: " << result.error().message;
    
    // Verify data is visible after commit
    result = conn->execute_raw(select.to_sql(), select.bind_params());
    ASSERT_TRUE(result) << "Failed to select after commit: " << result.error().message;
    
    auto& after_commit = *result;
    EXPECT_EQ(2, *after_commit[0].get<int>(0)) << "Expected 2 categories after commit";
    
    // Test error handling within transaction
    result = conn->begin_transaction();
    ASSERT_TRUE(result) << "Failed to begin third transaction: " << result.error().message;
    
    // Insert valid data
    auto insert_valid = insert_into(product)
        .columns(product.id, product.category_id, product.name, product.price, product.sku)
        .values(1, 1, "Smartphone", 999.99, "PHONE1");
        
    result = conn->execute_raw(insert_valid.to_sql(), insert_valid.bind_params());
    ASSERT_TRUE(result) << "Failed to insert valid product: " << result.error().message;
    
    // Try to insert invalid data (violating foreign key constraint)
    auto insert_invalid = insert_into(product)
        .columns(product.id, product.category_id, product.name, product.price, product.sku)
        .values(2, 999, "Invalid", 99.99, "INVALID");
        
    result = conn->execute_raw(insert_invalid.to_sql(), insert_invalid.bind_params());
    EXPECT_FALSE(result) << "Should fail to insert invalid product";
    
    // Verify transaction is still active
    EXPECT_TRUE(conn->in_transaction()) << "Transaction should still be active after error";
    
    // Rollback after error
    result = conn->rollback_transaction();
    ASSERT_TRUE(result) << "Failed to rollback after error: " << result.error().message;
    
    // Verify no products were inserted
    auto select_products = select(count_all()).from(product);
    result = conn->execute_raw(select_products.to_sql(), select_products.bind_params());
    ASSERT_TRUE(result) << "Failed to select products after rollback: " << result.error().message;
    
    auto& product_count = *result;
    EXPECT_EQ(0, *product_count[0].get<int>(0)) << "Expected 0 products after rollback";
} 