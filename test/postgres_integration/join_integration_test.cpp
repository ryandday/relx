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

// Test fixture for join integration tests
class JoinIntegrationTest : public ::testing::Test {
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
        auto result = relx::schema::create_table(conn.get(), category);
        ASSERT_TRUE(result) << "Failed to create category table: " << result.error().message;
        
        result = relx::schema::create_table(conn.get(), product);
        ASSERT_TRUE(result) << "Failed to create product table: " << result.error().message;
        
        result = relx::schema::create_table(conn.get(), customer);
        ASSERT_TRUE(result) << "Failed to create customer table: " << result.error().message;
        
        result = relx::schema::create_table(conn.get(), order);
        ASSERT_TRUE(result) << "Failed to create order table: " << result.error().message;
    }
    
    void insert_test_data() {
        using namespace relx::query;
        
        // Insert categories
        auto insert_categories = insert_into(category)
            .columns(category.id, category.name, category.description)
            .values(1, "Electronics", "Electronic devices and accessories")
            .values(2, "Clothing", "Apparel and fashion items")
            .values(3, "Books", "Books and publications")
            .values(4, "Empty Category", "Category with no products");
            
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
            .values(3, "Bob Johnson", "bob@example.com", std::nullopt)
            .values(4, "Alice Brown", "alice@example.com", "555-9012");
            
        result = conn->execute_raw(insert_customers.to_sql(), insert_customers.bind_params());
        ASSERT_TRUE(result) << "Failed to insert customers: " << result.error().message;
        
        // Insert orders (customer 4 has no orders)
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

// Test inner join
TEST_F(JoinIntegrationTest, InnerJoin) {
    using namespace relx::query;
    
    // Join orders with customers
    auto query = select(
        order.id,
        customer.name,
        product.name, 
        order.total
    )
    .from(order)
    .join(customer, order.customer_id == customer.id)
    .join(product, order.product_id == product.id)
    .order_by(order.id);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute inner join query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(6, rows.size()) << "Expected 6 orders with inner join";
    
    // Verify a few rows
    EXPECT_EQ(1, *rows[0].get<int>(0));
    EXPECT_EQ("John Doe", *rows[0].get<std::string>(1));
    EXPECT_EQ("Smartphone", *rows[0].get<std::string>(2));
    EXPECT_DOUBLE_EQ(999.99, *rows[0].get<double>(3));
    
    EXPECT_EQ(3, *rows[2].get<int>(0));
    EXPECT_EQ("Jane Smith", *rows[2].get<std::string>(1));
    EXPECT_EQ("Laptop", *rows[2].get<std::string>(2));
    EXPECT_DOUBLE_EQ(1299.99, *rows[2].get<double>(3));
}

// Test left join
TEST_F(JoinIntegrationTest, LeftJoin) {
    using namespace relx::query;
    
    // Left join customers with orders to find customers with no orders
    auto query = select(
        customer.id,
        customer.name,
        order.id.as("order_id")
    )
    .from(customer)
    .left_join(order, customer.id == order.customer_id)
    .order_by(customer.id, order.id);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute left join query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(7, rows.size()) << "Expected 7 rows from left join (6 orders + 1 customer with no orders)";
    
    // Last row should be customer 4 (Alice) with a NULL order_id
    auto last_customer_id = rows[6].get<int>(0);
    auto last_customer_name = rows[6].get<std::string>(1);
    auto last_order_id = rows[6].get<std::optional<int>>(2);
    
    ASSERT_TRUE(last_customer_id);
    ASSERT_TRUE(last_customer_name);
    
    EXPECT_EQ(4, *last_customer_id);
    EXPECT_EQ("Alice Brown", *last_customer_name);
    EXPECT_FALSE(last_order_id.has_value()) << "Expected NULL order_id for customer with no orders";
}

// Test right join
TEST_F(JoinIntegrationTest, RightJoin) {
    using namespace relx::query;
    
    // Right join products with categories to find all categories, including those with no products
    auto query = select(
        category.id,
        category.name,
        product.id.as("product_id"),
        product.name.as("product_name")
    )
    .from(product)
    .right_join(category, product.category_id == category.id)
    .order_by(category.id, product.id);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute right join query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(7, rows.size()) << "Expected 7 rows from right join (6 products + 1 category with no products)";
    
    // Last row should be category 4 (Empty Category) with NULL product values
    auto last_category_id = rows[6].get<int>(0);
    auto last_category_name = rows[6].get<std::string>(1);
    auto last_product_id = rows[6].get<std::optional<int>>(2);
    auto last_product_name = rows[6].get<std::optional<std::string>>(3);
    
    ASSERT_TRUE(last_category_id);
    ASSERT_TRUE(last_category_name);
    
    EXPECT_EQ(4, *last_category_id);
    EXPECT_EQ("Empty Category", *last_category_name);
    EXPECT_FALSE(last_product_id.has_value()) << "Expected NULL product_id for empty category";
    EXPECT_FALSE(last_product_name.has_value()) << "Expected NULL product_name for empty category";
}

// Test full outer join
TEST_F(JoinIntegrationTest, FullOuterJoin) {
    using namespace relx::query;
    
    // Full outer join customers and orders to find both customers with no orders
    // and orders with no customers (which won't exist in our data but demonstrates the join)
    auto query = select(
        customer.id.as("customer_id"),
        customer.name,
        order.id.as("order_id"),
        order.status
    )
    .from(customer)
    .full_join(order, customer.id == order.customer_id)
    .order_by(customer.id, order.id);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute full outer join query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(7, rows.size()) << "Expected 7 rows from full outer join";
    
    // Verify last row is customer 4 with NULL order details
    auto last_row = rows[rows.size() - 1];
    auto last_customer_id = last_row.get<int>(0);
    auto last_customer_name = last_row.get<std::string>(1);
    auto last_order_id = last_row.get<std::optional<int>>(2);
    auto last_order_status = last_row.get<std::optional<std::string>>(3);
    
    ASSERT_TRUE(last_customer_id);
    ASSERT_TRUE(last_customer_name);
    
    EXPECT_EQ(4, *last_customer_id);
    EXPECT_EQ("Alice Brown", *last_customer_name);
    EXPECT_FALSE(last_order_id.has_value());
    EXPECT_FALSE(last_order_status.has_value());
}

// Test complex joins with multiple tables
TEST_F(JoinIntegrationTest, ComplexJoins) {
    using namespace relx::query;
    
    // Join all tables together
    auto query = select(
        order.id.as("order_id"),
        customer.name.as("customer_name"),
        product.name.as("product_name"),
        category.name.as("category_name"),
        order.quantity,
        order.total,
        order.status
    )
    .from(order)
    .join(customer, order.customer_id == customer.id)
    .join(product, order.product_id == product.id)
    .join(category, product.category_id == category.id)
    .order_by(order.id);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute complex join query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(6, rows.size()) << "Expected 6 orders in complex join";
    
    // Verify first row
    auto first_row = rows[0];
    EXPECT_EQ(1, *first_row.get<int>("order_id"));
    EXPECT_EQ("John Doe", *first_row.get<std::string>("customer_name"));
    EXPECT_EQ("Smartphone", *first_row.get<std::string>("product_name"));
    EXPECT_EQ("Electronics", *first_row.get<std::string>("category_name"));
    EXPECT_EQ(1, *first_row.get<int>("quantity"));
    EXPECT_DOUBLE_EQ(999.99, *first_row.get<double>("total"));
    EXPECT_EQ("delivered", *first_row.get<std::string>("status"));
}

// Test self join
TEST_F(JoinIntegrationTest, SelfJoin) {
    using namespace relx::query;
    
    // Find products in the same category
    auto p1 = product.as("p1");
    auto p2 = product.as("p2");
    
    auto query = select(
        p1.id.as("product1_id"),
        p1.name.as("product1_name"),
        p2.id.as("product2_id"),
        p2.name.as("product2_name"),
        p1.category_id
    )
    .from(p1)
    .join(p2, p1.category_id == p2.category_id && p1.id != p2.id)
    .order_by(p1.category_id, p1.id, p2.id);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute self join query: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(6, rows.size()) << "Expected 6 product pairs (2 in Electronics, 2 in Clothing, 2 in Books)";
    
    // Check each pair
    // Electronics
    EXPECT_EQ(1, *rows[0].get<int>("product1_id"));
    EXPECT_EQ(2, *rows[0].get<int>("product2_id"));
    EXPECT_EQ(1, *rows[0].get<int>("category_id"));
    
    EXPECT_EQ(2, *rows[1].get<int>("product1_id"));
    EXPECT_EQ(1, *rows[1].get<int>("product2_id"));
    EXPECT_EQ(1, *rows[1].get<int>("category_id"));
    
    // Clothing
    EXPECT_EQ(3, *rows[2].get<int>("product1_id"));
    EXPECT_EQ(4, *rows[2].get<int>("product2_id"));
    EXPECT_EQ(2, *rows[2].get<int>("category_id"));
    
    EXPECT_EQ(4, *rows[3].get<int>("product1_id"));
    EXPECT_EQ(3, *rows[3].get<int>("product2_id"));
    EXPECT_EQ(2, *rows[3].get<int>("category_id"));
    
    // Books
    EXPECT_EQ(5, *rows[4].get<int>("product1_id"));
    EXPECT_EQ(6, *rows[4].get<int>("product2_id"));
    EXPECT_EQ(3, *rows[4].get<int>("category_id"));
    
    EXPECT_EQ(6, *rows[5].get<int>("product1_id"));
    EXPECT_EQ(5, *rows[5].get<int>("product2_id"));
    EXPECT_EQ(3, *rows[5].get<int>("category_id"));
}

// Test subqueries
TEST_F(JoinIntegrationTest, Subqueries) {
    using namespace relx::query;
    
    // Subquery in FROM clause - get products more expensive than average
    auto avg_price_query = select_expr(avg(product.price)).from(product);
    
    auto query = select(
        product.id,
        product.name,
        product.price
    )
    .from(product)
    .where(product.price > avg_price_query)
    .order_by(product.price);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute subquery: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(2, rows.size()) << "Expected 2 products above average price";
    
    // Should be the smartphone and laptop
    EXPECT_EQ("Smartphone", *rows[0].get<std::string>(1));
    EXPECT_EQ("Laptop", *rows[1].get<std::string>(1));
    
    // Subquery with EXISTS - find customers who have orders
    auto customers_with_orders = select(customer.id, customer.name)
        .from(customer)
        .where(exists(
            select(order.id)
            .from(order)
            .where(order.customer_id == customer.id)
        ))
        .order_by(customer.id);
    
    result = conn->execute_raw(customers_with_orders.to_sql(), customers_with_orders.bind_params());
    ASSERT_TRUE(result) << "Failed to execute EXISTS subquery: " << result.error().message;
    
    auto& customer_rows = *result;
    ASSERT_EQ(3, customer_rows.size()) << "Expected 3 customers with orders";
    
    // Should be customers 1, 2, and 3
    EXPECT_EQ(1, *customer_rows[0].get<int>(0));
    EXPECT_EQ(2, *customer_rows[1].get<int>(0));
    EXPECT_EQ(3, *customer_rows[2].get<int>(0));
}

// Test join with conditional expressions
TEST_F(JoinIntegrationTest, JoinWithConditions) {
    using namespace relx::query;
    
    // Join with complex condition
    auto query = select(
        order.id,
        customer.name,
        product.name,
        product.price,
        order.quantity,
        order.total
    )
    .from(order)
    .join(customer, order.customer_id == customer.id)
    .join(product, order.product_id == product.id)
    .where((product.price > 50) || (order.quantity > 1))
    .order_by(order.id);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute join with conditions: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(5, rows.size()) << "Expected 5 orders meeting the condition";
    
    // Order IDs should be 1, 2, 3, 4, 5 (all except order 2 which is for a T-shirt with quantity 1)
    std::vector<int> expected_order_ids = {1, 2, 3, 4, 5};
    for (size_t i = 0; i < rows.size(); ++i) {
        EXPECT_EQ(expected_order_ids[i], *rows[i].get<int>(0));
    }
}

// Test join with aggregates
TEST_F(JoinIntegrationTest, JoinWithAggregates) {
    using namespace relx::query;
    
    // For each category, get the number of products and the average price
    auto query = select(
        category.id,
        category.name,
        as(count(product.id), "product_count"),
        as(avg(product.price), "avg_price")
    )
    .from(category)
    .left_join(product, category.id == product.category_id)
    .group_by(category.id, category.name)
    .order_by(category.id);
    
    auto result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute join with aggregates: " << result.error().message;
    
    auto& rows = *result;
    ASSERT_EQ(4, rows.size()) << "Expected 4 categories";
    
    // Check counts and averages
    // Category 1: Electronics - 2 products, avg price ~1149.99
    EXPECT_EQ(1, *rows[0].get<int>(0));
    EXPECT_EQ("Electronics", *rows[0].get<std::string>(1));
    EXPECT_EQ(2, *rows[0].get<int>(2));
    EXPECT_NEAR(1149.99, *rows[0].get<double>(3), 0.01);
    
    // Category 2: Clothing - 2 products, avg price ~34.99
    EXPECT_EQ(2, *rows[1].get<int>(0));
    EXPECT_EQ("Clothing", *rows[1].get<std::string>(1));
    EXPECT_EQ(2, *rows[1].get<int>(2));
    EXPECT_NEAR(34.99, *rows[1].get<double>(3), 0.01);
    
    // Category 3: Books - 2 products, avg price ~47.49
    EXPECT_EQ(3, *rows[2].get<int>(0));
    EXPECT_EQ("Books", *rows[2].get<std::string>(1));
    EXPECT_EQ(2, *rows[2].get<int>(2));
    EXPECT_NEAR(47.49, *rows[2].get<double>(3), 0.01);
    
    // Category 4: Empty Category - 0 products, NULL avg price
    EXPECT_EQ(4, *rows[3].get<int>(0));
    EXPECT_EQ("Empty Category", *rows[3].get<std::string>(1));
    EXPECT_EQ(0, *rows[3].get<int>(2));
    EXPECT_FALSE(rows[3].get<double>(3).has_value());
} 