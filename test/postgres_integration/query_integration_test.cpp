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

// Test fixture for query integration tests
class QueryIntegrationTest : public ::testing::Test {
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
        conn = std::make_unique<Connection>("host=localhost port=5434 dbname=sqllib_test user=postgres password=postgres");
        auto result = conn->connect();
        ASSERT_TRUE(result) << "Failed to connect: " << result.error().message;
        
        // Clean up any existing tables
        cleanup_database();
        
        // Set up the schema
        setup_schema();
        
        // Insert test data
        insert_test_data();
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
        auto sql = relx::schema::create_table(category);
        auto result = conn->execute_raw(sql);
        ASSERT_TRUE(result) << "Failed to create category table: " << result.error().message;
        
        sql = relx::schema::create_table(product);
        result = conn->execute_raw(sql);
        ASSERT_TRUE(result) << "Failed to create product table: " << result.error().message;
        
        sql = relx::schema::create_table(customer);
        result = conn->execute_raw(sql);
        ASSERT_TRUE(result) << "Failed to create customer table: " << result.error().message;
        
        sql = relx::schema::create_table(order);
        result = conn->execute_raw(sql);
        ASSERT_TRUE(result) << "Failed to create order table: " << result.error().message;
    }
    
    void insert_test_data() {
        using namespace relx::query;
        
        // Insert categories
        auto insert_categories = insert_into(category)
            .columns(category.id, category.name, category.description)
            .values(1, "Electronics", "Electronic devices and accessories")
            .values(2, "Clothing", "Apparel and fashion items")
            .values(3, "Books", "Books and publications");
            
        auto result = conn->execute_raw(insert_categories.to_sql(), insert_categories.bind_params());
        ASSERT_TRUE(result) << "Failed to insert categories: " << result.error().message;
        
        // Insert products
        auto insert_products = insert_into(product)
            .columns(product.id, product.category_id, product.name, product.description, product.price, product.sku)
            .values(1, 1, "Smartphone", "Latest model smartphone", 999.99, "ELEC001")
            .values(2, 1, "Laptop", "High-performance laptop", 1299.99, "ELEC002")
            .values(3, 2, "T-Shirt", "Cotton t-shirt", 19.99, "CLTH001")
            .values(4, 2, "Jeans", "Denim jeans", 49.99, "CLTH002")
            .values(5, 3, "Novel", "Bestselling fiction novel", 14.99, "BOOK001")
            .values(6, 3, "Textbook", "Computer Science textbook", 79.99, "BOOK002");
            
        result = conn->execute_raw(insert_products.to_sql(), insert_products.bind_params());
        ASSERT_TRUE(result) << "Failed to insert products: " << result.error().message;
        
        // Insert customers
        auto insert_customers = insert_into(customer)
            .columns(customer.id, customer.name, customer.email, customer.phone)
            .values(1, "John Doe", "john@example.com", "555-1234")
            .values(2, "Jane Smith", "jane@example.com", "555-5678")
            .values(3, "Bob Johnson", "bob@example.com", std::nullopt);
            
        result = conn->execute_raw(insert_customers.to_sql(), insert_customers.bind_params());
        ASSERT_TRUE(result) << "Failed to insert customers: " << result.error().message;
        
        // Insert orders
        auto insert_orders = insert_into(order)
            .columns(order.id, order.customer_id, order.product_id, order.quantity, order.total, order.status)
            .values(1, 1, 1, 1, 999.99, "delivered")
            .values(2, 1, 3, 2, 39.98, "delivered")
            .values(3, 2, 2, 1, 1299.99, "shipped")
            .values(4, 2, 5, 3, 44.97, "processing")
            .values(5, 3, 6, 1, 79.99, "pending")
            .values(6, 3, 4, 1, 49.99, "cancelled");
            
        result = conn->execute_raw(insert_orders.to_sql(), insert_orders.bind_params());
        ASSERT_TRUE(result) << "Failed to insert orders: " << result.error().message;
    }
};

// Test basic SELECT queries
TEST_F(QueryIntegrationTest, BasicSelect) {
    using namespace relx::query;
    
    // Simple select all from a table
    auto query = select(product.id, product.name, product.price)
        .from(product)
        .order_by(product.id);
        
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(6, rows.size()) << "Expected 6 products";
    
    // Verify first row
    EXPECT_EQ(1, *rows[0].get<int>(0));
    EXPECT_EQ("Smartphone", *rows[0].get<std::string>(1));
    EXPECT_DOUBLE_EQ(999.99, *rows[0].get<double>(2));
}

// Test WHERE clause filtering
TEST_F(QueryIntegrationTest, WhereClauseFiltering) {
    using namespace relx::query;
    
    // Filter with a simple condition
    auto query = select(product.id, product.name, product.price)
        .from(product)
        .where(product.category_id == 1)
        .order_by(product.id);
        
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(2, rows.size()) << "Expected 2 electronics products";
    
    // Filter with a complex condition
    auto complex_query = select(product.id, product.name, product.price)
        .from(product)
        .where((product.price > 50) && (product.price < 1000))
        .order_by(product.price);
        
    result = conn->execute_raw(complex_query.to_sql(), complex_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute complex query: " << result.error().message;
    
    auto& complex_rows = *result;
    ASSERT_EQ(2, complex_rows.size()) << "Expected 2 products with price between 50 and 1000";
    
    // First row should be the textbook (79.99)
    EXPECT_EQ("Textbook", *complex_rows[0].get<std::string>(1));
    
    // Second row should be the smartphone (999.99)
    EXPECT_EQ("Smartphone", *complex_rows[1].get<std::string>(1));
}

// Test ORDER BY clause
TEST_F(QueryIntegrationTest, OrderByClause) {
    using namespace relx::query;
    
    // Test ascending order
    auto asc_query = select(product.name, product.price)
        .from(product)
        .order_by(product.price);  // Ascending
        
    auto result = conn->execute_raw(asc_query.to_sql(), asc_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute ascending query: " << result.error().message;
    
    auto& asc_rows = *result;
    ASSERT_EQ(6, asc_rows.size());
    
    // First should be the cheapest (Novel, 14.99)
    EXPECT_EQ("Novel", *asc_rows[0].get<std::string>(0));
    
    // Last should be the most expensive (Laptop, 1299.99)
    EXPECT_EQ("Laptop", *asc_rows[5].get<std::string>(0));
    
    // Test descending order
    auto desc_query = select(product.name, product.price)
        .from(product)
        .order_by(relx::desc(product.price));  // Descending
        
    result = conn->execute_raw(desc_query.to_sql(), desc_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute descending query: " << result.error().message;
    
    auto& desc_rows = *result;
    ASSERT_EQ(6, desc_rows.size());
    
    // First should be the most expensive (Laptop, 1299.99)
    EXPECT_EQ("Laptop", *desc_rows[0].get<std::string>(0));
    
    // Last should be the cheapest (Novel, 14.99)
    EXPECT_EQ("Novel", *desc_rows[5].get<std::string>(0));
    
    // Test multiple order by columns
    auto multi_query = select(product.category_id, product.name, product.price)
        .from(product)
        .order_by(product.category_id)  // Ascending category_id
        .order_by(relx::desc(product.price));       // Descending price within category
        
    result = conn->execute_raw(multi_query.to_sql(), multi_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute multi-order query: " << result.error().message;
    
    auto& multi_rows = *result;
    ASSERT_EQ(6, multi_rows.size());
    
    // Should be ordered by category_id, then by price descending within each category
    
    // First two rows should be category 1 (Electronics), with Laptop first (more expensive)
    EXPECT_EQ(1, *multi_rows[0].get<int>(0));
    EXPECT_EQ("Laptop", *multi_rows[0].get<std::string>(1));
    
    EXPECT_EQ(1, *multi_rows[1].get<int>(0));
    EXPECT_EQ("Smartphone", *multi_rows[1].get<std::string>(1));
    
    // Next two rows should be category 2 (Clothing), with Jeans first (more expensive)
    EXPECT_EQ(2, *multi_rows[2].get<int>(0));
    EXPECT_EQ("Jeans", *multi_rows[2].get<std::string>(1));
    
    EXPECT_EQ(2, *multi_rows[3].get<int>(0));
    EXPECT_EQ("T-Shirt", *multi_rows[3].get<std::string>(1));
}

// Test LIMIT and OFFSET clauses
TEST_F(QueryIntegrationTest, LimitAndOffset) {
    using namespace relx::query;
    
    // Test with limit
    auto limit_query = select(product.id, product.name)
        .from(product)
        .order_by(product.id)
        .limit(3);
        
    auto result = conn->execute_raw(limit_query.to_sql(), limit_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute limit query: " << result.error().message;
    
    auto& limit_rows = *result;
    ASSERT_EQ(3, limit_rows.size()) << "Expected 3 products with LIMIT 3";
    
    // Should be the first 3 products by ID
    EXPECT_EQ(1, *limit_rows[0].get<int>(0));
    EXPECT_EQ(2, *limit_rows[1].get<int>(0));
    EXPECT_EQ(3, *limit_rows[2].get<int>(0));
    
    // Test with limit and offset
    auto offset_query = select(product.id, product.name)
        .from(product)
        .order_by(product.id)
        .limit(2)
        .offset(3);
        
    result = conn->execute_raw(offset_query.to_sql(), offset_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute offset query: " << result.error().message;
    
    auto& offset_rows = *result;
    ASSERT_EQ(2, offset_rows.size()) << "Expected 2 products with LIMIT 2 OFFSET 3";
    
    // Should be products 4 and 5
    EXPECT_EQ(4, *offset_rows[0].get<int>(0));
    EXPECT_EQ(5, *offset_rows[1].get<int>(0));
    
    // Test with just offset
    auto just_offset_query = select(product.id, product.name)
        .from(product)
        .order_by(product.id)
        .offset(5);
        
    result = conn->execute_raw(just_offset_query.to_sql(), just_offset_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute just-offset query: " << result.error().message;
    
    auto& just_offset_rows = *result;
    ASSERT_EQ(1, just_offset_rows.size()) << "Expected 1 product with OFFSET 5";
    
    // Should be just product 6
    EXPECT_EQ(6, *just_offset_rows[0].get<int>(0));
}

// Test aggregate functions
TEST_F(QueryIntegrationTest, AggregateFunctions) {
    using namespace relx::query;
    
    // Test COUNT
    auto count_query = select_expr(count(product.id))
        .from(product)
        .where(product.category_id == 1);
        
    auto result = conn->execute_raw(count_query.to_sql(), count_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute count query: " << result.error().message;
    
    auto& count_row = *result;
    ASSERT_EQ(1, count_row.size());
    EXPECT_EQ(2, *count_row[0].get<int>(0)) << "Expected 2 products in category 1";
    
    // Test SUM
    auto sum_query = select_expr(sum(order.total))
        .from(order)
        .where(order.customer_id == 1);
        
    result = conn->execute_raw(sum_query.to_sql(), sum_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute sum query: " << result.error().message;
    
    auto& sum_row = *result;
    ASSERT_EQ(1, sum_row.size());
    EXPECT_DOUBLE_EQ(1039.97, *sum_row[0].get<double>(0)) << "Expected sum of 1039.97 for customer 1's orders";
    
    // Test AVG
    auto avg_query = select_expr(avg(product.price))
        .from(product);
        
    result = conn->execute_raw(avg_query.to_sql(), avg_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute avg query: " << result.error().message;
    
    auto& avg_row = *result;
    ASSERT_EQ(1, avg_row.size());
    
    // Average of 999.99, 1299.99, 19.99, 49.99, 14.99, 79.99 = 410.82333...
    double expected_avg = (999.99 + 1299.99 + 19.99 + 49.99 + 14.99 + 79.99) / 6.0;
    EXPECT_NEAR(expected_avg, *avg_row[0].get<double>(0), 0.01) << "Expected average price around 410.82";
    
    // Test MIN/MAX
    auto min_max_query = select_expr(min(product.price), max(product.price))
        .from(product);
        
    result = conn->execute_raw(min_max_query.to_sql(), min_max_query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute min/max query: " << result.error().message;
    
    auto& min_max_row = *result;
    ASSERT_EQ(1, min_max_row.size());
    EXPECT_DOUBLE_EQ(14.99, *min_max_row[0].get<double>(0)) << "Expected min price 14.99";
    EXPECT_DOUBLE_EQ(1299.99, *min_max_row[0].get<double>(1)) << "Expected max price 1299.99";
}

// Test multiple aliases and expressions
TEST_F(QueryIntegrationTest, AliasesAndExpressions) {
    using namespace relx::query;
    
    auto query = select_expr(
        as(count_all(), "total_count"),
        as(sum(product.price), "total_price"),
        as(avg(product.price), "avg_price"),
        as(min(product.price), "min_price"),
        as(max(product.price), "max_price")
    )
    .from(product);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute alias query: " << result.error().message;
    
    auto& row = *result;
    ASSERT_EQ(1, row.size());
    
    // Check all aggregates
    EXPECT_EQ(6, *row[0].get<int>("total_count")) << "Expected 6 products";
    
    double total = 999.99 + 1299.99 + 19.99 + 49.99 + 14.99 + 79.99;
    EXPECT_NEAR(total, *row[0].get<double>("total_price"), 0.01) << "Expected total price around 2464.94";
    
    double avg = total / 6.0;
    EXPECT_NEAR(avg, *row[0].get<double>("avg_price"), 0.01) << "Expected average price around 410.82";
    
    EXPECT_DOUBLE_EQ(14.99, *row[0].get<double>("min_price")) << "Expected min price 14.99";
    EXPECT_DOUBLE_EQ(1299.99, *row[0].get<double>("max_price")) << "Expected max price 1299.99";
}

// Test GROUP BY clause
TEST_F(QueryIntegrationTest, GroupBy) {
    using namespace relx::query;
    
    // Group orders by customer_id and count them
    auto query = select_expr(
        order.customer_id,
        as(count_all(), "order_count"),
        as(sum(order.total), "total_spent")
    )
    .from(order)
    .group_by(order.customer_id)
    .order_by(order.customer_id);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute group by query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(3, rows.size()) << "Expected 3 customer groups";
    
    // Customer 1 has 2 orders totaling 1039.97
    EXPECT_EQ(1, *rows[0].get<int>(0));
    EXPECT_EQ(2, *rows[0].get<int>(1));
    EXPECT_NEAR(1039.97, *rows[0].get<double>(2), 0.01);
    
    // Customer 2 has 2 orders totaling 1344.96
    EXPECT_EQ(2, *rows[1].get<int>(0));
    EXPECT_EQ(2, *rows[1].get<int>(1));
    EXPECT_NEAR(1344.96, *rows[1].get<double>(2), 0.01);
    
    // Customer 3 has 2 orders totaling 129.98
    EXPECT_EQ(3, *rows[2].get<int>(0));
    EXPECT_EQ(2, *rows[2].get<int>(1));
    EXPECT_NEAR(129.98, *rows[2].get<double>(2), 0.01);
}

// Test HAVING clause
TEST_F(QueryIntegrationTest, Having) {
    using namespace relx::query;
    
    // Group products by category and filter by average price
    auto query = select_expr(
        product.category_id,
        as(count_all(), "product_count"),
        as(avg(product.price), "avg_price")
    )
    .from(product)
    .group_by(product.category_id)
    .having(avg(product.price) > 100)
    .order_by(product.category_id);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute having query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(1, rows.size()) << "Expected 1 category with avg price > 100";
    
    // Category 1 (Electronics) should be the only one with avg price > 100
    EXPECT_EQ(1, *rows[0].get<int>(0));
    EXPECT_EQ(2, *rows[0].get<int>(1)) << "Expected 2 products in category 1";
    EXPECT_NEAR(1149.99, *rows[0].get<double>(2), 0.01) << "Expected avg price around 1149.99";
} 