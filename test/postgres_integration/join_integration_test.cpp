#include <gtest/gtest.h>
#include <relx/schema.hpp>
#include <relx/query.hpp>
#include <relx/connection.hpp>
#include <relx/postgresql.hpp>
#include <string>
#include <memory>
#include <optional>
#include <vector>
#include <iostream>

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
            .values(3, "Books", "Books and publications")
            .values(4, "Empty Category", "Category with no products");
            
        auto result = conn->execute_raw(insert_categories.to_sql(), insert_categories.bind_params());
        ASSERT_TRUE(result) << "Failed to insert categories: " << result.error().message;
        
        // Verify category 4 exists
        auto check_category = select(category.id, category.name)
            .from(category)
            .where(category.id == 4);
        
        result = conn->execute_raw(check_category.to_sql(), check_category.bind_params());
        ASSERT_TRUE(result) << "Failed to execute check_category query";
        ASSERT_EQ(1, result->size()) << "Category 4 should exist";
        
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
        
        // Verify customer 4 exists
        auto check_customer = select(customer.id, customer.name)
            .from(customer)
            .where(customer.id == 4);
        
        result = conn->execute_raw(check_customer.to_sql(), check_customer.bind_params());
        ASSERT_TRUE(result) << "Failed to execute check_customer query";
        ASSERT_EQ(1, result->size()) << "Customer 4 should exist";
        
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
        
        // Verify customer 4 has no orders
        auto check_orders = select(order.id)
            .from(order)
            .where(order.customer_id == 4);
        
        result = conn->execute_raw(check_orders.to_sql(), check_orders.bind_params());
        ASSERT_TRUE(result) << "Failed to execute check_orders query";
        ASSERT_EQ(0, result->size()) << "Customer 4 should have no orders";
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
    
    auto left_join_query = relx::query::select(
        customer.id,
        customer.name,
        relx::as(order.id, "order_id")
    )
    .from(customer)
    .left_join(order, customer.id == order.customer_id);
    auto result = conn->execute(left_join_query);
    ASSERT_TRUE(result) << "Failed to execute direct SQL left join";
    
    auto& rows = *result;
    
    // Find customer with id 4 (Alice) who should have NULL order_id
    bool found_alice_with_null_order = false;
    for (size_t i = 0; i < rows.size(); i++) {
        auto& row = rows[i];
        auto cust_id = row.get<int>(0);
        
        if (cust_id && *cust_id == 4) {
            // Check if order_id is NULL by using std::optional<int>
            auto order_id = row.get<std::optional<int>>(2);
            if (order_id && !order_id->has_value()) {
                auto name = row.get<std::string>(1);
                ASSERT_TRUE(name.has_value());
                EXPECT_EQ("Alice Brown", *name);
                found_alice_with_null_order = true;
                break;
            }
        }
    }
    
    ASSERT_TRUE(found_alice_with_null_order) << "Alice should have a NULL order_id in LEFT JOIN results";
    
    // Now test the query builder version
    auto query = select(
        customer.id,
        customer.name,
        relx::as(order.id, "order_id")
    )
    .from(customer)
    .left_join(order, customer.id == order.customer_id)
    .order_by(customer.id)
    .order_by(order.id);
    
    result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute left join query: " << result.error().message;
    
    rows = *result;
    
    // Verify we find Alice with null order_id in query builder version too
    bool found_alice_in_builder = false;
    for (const auto& row : rows) {
        auto cust_id = row.get<int>(0);
        if (cust_id && *cust_id == 4) {
            // Check if order_id is NULL by using std::optional<int>
            auto order_id = row.get<std::optional<int>>(2);
            if (order_id && !order_id->has_value()) {
                auto name = row.get<std::string>(1);
                ASSERT_TRUE(name.has_value());
                EXPECT_EQ("Alice Brown", *name);
                found_alice_in_builder = true;
                break;
            }
        }
    }
    
    ASSERT_TRUE(found_alice_in_builder) << "Alice should have a NULL order_id in query builder LEFT JOIN results";
}

// Test right join
TEST_F(JoinIntegrationTest, RightJoin) {
    using namespace relx::query;
    
    // Run a direct SQL right join to verify the expected results
    auto direct_sql = "SELECT c.id, c.name, p.id AS product_id, p.name AS product_name FROM products p "
                     "RIGHT JOIN categories c ON p.category_id = c.id "
                     "ORDER BY c.id, p.id";
    
    auto right_join_query = relx::query::select(
        category.id,
        category.name,
        relx::as(product.id, "product_id"),
        relx::as(product.name, "product_name"))
    .from(product)
    .right_join(category, product.category_id == category.id)
    .order_by(category.id)
    .order_by(product.id);
    
    auto result = conn->execute(right_join_query);
    ASSERT_TRUE(result) << "Failed to execute direct SQL right join";
    
    auto& rows = *result;
    
    // Find category with id 4 ("Empty Category") which should have NULL product values
    bool found_empty_category_direct = false;
    for (size_t i = 0; i < rows.size(); i++) {
        auto& row = rows[i];
        auto cat_id = row.get<int>(0);
        
        if (cat_id && *cat_id == 4) {
            // Check if product_id is NULL by using std::optional<int>
            auto product_id = row.get<std::optional<int>>(2);
            auto product_name = row.get<std::optional<std::string>>(3);
            
            if (product_id && !product_id->has_value() && 
                product_name && !product_name->has_value()) {
                auto cat_name = row.get<std::string>(1);
                ASSERT_TRUE(cat_name.has_value());
                EXPECT_EQ("Empty Category", *cat_name);
                found_empty_category_direct = true;
                break;
            }
        }
    }
    
    ASSERT_TRUE(found_empty_category_direct) << "Empty Category should have NULL product values in RIGHT JOIN results";
    
    // Now test the query builder version
    auto query = select(
        category.id,
        category.name,
        relx::as(product.id, "product_id"),
        relx::as(product.name, "product_name")
    )
    .from(product)
    .right_join(category, product.category_id == category.id)
    .order_by(category.id)
    .order_by(product.id);
    
    result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute right join query: " << result.error().message;
    
    rows = *result;
    
    // Verify we find Empty Category with null product values in query builder version too
    bool found_empty_category_builder = false;
    for (const auto& row : rows) {
        auto cat_id = row.get<int>(0);
        if (cat_id && *cat_id == 4) {
            // Check if product_id and product_name are NULL by using std::optional<T>
            auto product_id = row.get<std::optional<int>>("product_id");
            auto product_name = row.get<std::optional<std::string>>("product_name");
            
            if (product_id && !product_id->has_value() && 
                product_name && !product_name->has_value()) {
                auto cat_name = row.get<std::string>(1);
                ASSERT_TRUE(cat_name.has_value());
                EXPECT_EQ("Empty Category", *cat_name);
                found_empty_category_builder = true;
                break;
            }
        }
    }
    
    ASSERT_TRUE(found_empty_category_builder) << "Empty Category should have NULL product values in query builder RIGHT JOIN results";
}

// Test full outer join
TEST_F(JoinIntegrationTest, FullOuterJoin) {
    using namespace relx::query;
    
    // Run a direct SQL full outer join to verify the expected results
    auto direct_sql = "SELECT c.id AS customer_id, c.name, o.id AS order_id, o.status FROM customers c "
                     "FULL OUTER JOIN orders o ON c.id = o.customer_id "
                     "ORDER BY c.id, o.id";
    
    auto full_outer_join_query = relx::query::select(
        relx::as(customer.id, "customer_id"),
        customer.name,
        relx::as(order.id, "order_id"),
        order.status
    ).from(customer)
    .full_join(order, customer.id == order.customer_id)
    .order_by(customer.id)
    .order_by(order.id);
    
    auto result = conn->execute(full_outer_join_query);
    ASSERT_TRUE(result) << "Failed to execute direct SQL full outer join";
    
    auto& rows = *result;
    
    // Find customer with id 4 (Alice) who should have NULL order_id and status
    bool found_alice_direct = false;
    for (size_t i = 0; i < rows.size(); i++) {
        auto& row = rows[i];
        auto cust_id = row.get<int>("customer_id");
        
        if (cust_id && *cust_id == 4) {
            // Check if order_id and status are NULL by using std::optional<T>
            auto order_id = row.get<std::optional<int>>("order_id");
            auto status = row.get<std::optional<std::string>>(3);
            
            if (order_id && !order_id->has_value() && 
                status && !status->has_value()) {
                auto name = row.get<std::string>(1);
                ASSERT_TRUE(name.has_value());
                EXPECT_EQ("Alice Brown", *name);
                found_alice_direct = true;
                break;
            }
        }
    }
    
    ASSERT_TRUE(found_alice_direct) << "Alice should have NULL order values in FULL OUTER JOIN results";
    
    // Now test the query builder version
    auto query = select(
        relx::as(customer.id, "customer_id"),
        customer.name,
        relx::as(order.id, "order_id"),
        order.status
    )
    .from(customer)
    .full_join(order, customer.id == order.customer_id)
    .order_by(customer.id)
    .order_by(order.id);
    
    result = conn->execute_raw(query.to_sql(), query.bind_params());
    ASSERT_TRUE(result) << "Failed to execute full join query: " << result.error().message;
    
    rows = *result;
    
    // Verify we find Alice with null order values in query builder version too
    bool found_alice_builder = false;
    for (const auto& row : rows) {
        auto cust_id = row.get<int>("customer_id");
        if (cust_id && *cust_id == 4) {
            // Check if order_id and status are NULL by using std::optional<T>
            auto order_id = row.get<std::optional<int>>("order_id");
            auto status = row.get<std::optional<std::string>>(3);
            
            if (order_id && !order_id->has_value() && 
                status && !status->has_value()) {
                auto name = row.get<std::string>(1);
                ASSERT_TRUE(name.has_value());
                EXPECT_EQ("Alice Brown", *name);
                found_alice_builder = true;
                break;
            }
        }
    }
    
    ASSERT_TRUE(found_alice_builder) << "Alice should have NULL order values in query builder FULL OUTER JOIN results";
}

// Test complex joins with multiple tables
TEST_F(JoinIntegrationTest, ComplexJoins) {
    using namespace relx::query;
    
    // Join all tables together
    auto query = select(
        relx::as(order.id, "order_id"),
        relx::as(customer.name, "customer_name"),
        relx::as(product.name, "product_name"),
        relx::as(category.name, "category_name"),
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

// TODO not supported yet
// Test self join
// TEST_F(JoinIntegrationTest, SelfJoin) {
//     using namespace relx::query;
    
//     // Find products in the same category
//     product product2;
    
//     auto query = select(
//         relx::as(product.id, "product1_id"),
//         relx::as(product.name, "product1_name"),
//         relx::as(product2.id, "product2_id"),
//         relx::as(product2.name, "product2_name"),
//         product.category_id
//     )
//     .from(product)
//     .join(product2, product.category_id == product2.category_id && product.id != product2.id)
//     .order_by(product.category_id)
//     .order_by(product.id)
//     .order_by(product2.id);
    
//     auto result = conn->execute_raw(query.to_sql(), query.bind_params());
//     ASSERT_TRUE(result) << "Failed to execute self join query: " << result.error().message;
    
//     auto& rows = *result;
//     ASSERT_EQ(6, rows.size()) << "Expected 6 product pairs (2 in Electronics, 2 in Clothing, 2 in Books)";
    
//     // Check each pair
//     // Electronics
//     EXPECT_EQ(1, *rows[0].get<int>("product1_id"));
//     EXPECT_EQ(2, *rows[0].get<int>("product2_id"));
//     EXPECT_EQ(1, *rows[0].get<int>("category_id"));
    
//     EXPECT_EQ(2, *rows[1].get<int>("product1_id"));
//     EXPECT_EQ(1, *rows[1].get<int>("product2_id"));
//     EXPECT_EQ(1, *rows[1].get<int>("category_id"));
    
//     // Clothing
//     EXPECT_EQ(3, *rows[2].get<int>("product1_id"));
//     EXPECT_EQ(4, *rows[2].get<int>("product2_id"));
//     EXPECT_EQ(2, *rows[2].get<int>("category_id"));
    
//     EXPECT_EQ(4, *rows[3].get<int>("product1_id"));
//     EXPECT_EQ(3, *rows[3].get<int>("product2_id"));
//     EXPECT_EQ(2, *rows[3].get<int>("category_id"));
    
//     // Books
//     EXPECT_EQ(5, *rows[4].get<int>("product1_id"));
//     EXPECT_EQ(6, *rows[4].get<int>("product2_id"));
//     EXPECT_EQ(3, *rows[4].get<int>("category_id"));
    
//     EXPECT_EQ(6, *rows[5].get<int>("product1_id"));
//     EXPECT_EQ(5, *rows[5].get<int>("product2_id"));
//     EXPECT_EQ(3, *rows[5].get<int>("category_id"));
// }

// Test subqueries
// TEST_F(JoinIntegrationTest, Subqueries) {
//     using namespace relx::query;
    
//     // Subquery in FROM clause - get products more expensive than average
//     auto avg_price_query = select_expr(avg(product.price)).from(product);
    
//     auto query = select(
//         product.id,
//         product.name,
//         product.price
//     )
//     .from(product)
//     // TODO: more overload support for subquery
//     .where(product.price > avg_price_query)
//     .order_by(product.price);
    
//     auto result = conn->execute_raw(query.to_sql(), query.bind_params());
//     ASSERT_TRUE(result) << "Failed to execute subquery: " << result.error().message;
    
//     auto& rows = *result;
//     ASSERT_EQ(2, rows.size()) << "Expected 2 products above average price";
    
//     // Should be the smartphone and laptop
//     EXPECT_EQ("Smartphone", *rows[0].get<std::string>(1));
//     EXPECT_EQ("Laptop", *rows[1].get<std::string>(1));
    
//     // Subquery with EXISTS - find customers who have orders
//     auto customers_with_orders = select(customer.id, customer.name)
//         .from(customer)
//         .where(exists(
//             select(order.id)
//             .from(order)
//             .where(order.customer_id == customer.id)
//         ))
//         .order_by(customer.id);
    
//     result = conn->execute_raw(customers_with_orders.to_sql(), customers_with_orders.bind_params());
//     ASSERT_TRUE(result) << "Failed to execute EXISTS subquery: " << result.error().message;
    
//     auto& customer_rows = *result;
//     ASSERT_EQ(3, customer_rows.size()) << "Expected 3 customers with orders";
    
//     // Should be customers 1, 2, and 3
//     EXPECT_EQ(1, *customer_rows[0].get<int>(0));
//     EXPECT_EQ(2, *customer_rows[1].get<int>(0));
//     EXPECT_EQ(3, *customer_rows[2].get<int>(0));
// }

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