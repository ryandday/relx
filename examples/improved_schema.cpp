#include <sqllib/schema.hpp>
#include <iostream>
#include <optional>
#include <string>

namespace sqllib {
namespace examples {

// Define a schema with the improved features
struct Product {
    // Basic columns
    schema::column<"id", int> id;
    schema::column<"name", std::string> name;
    
    // Nullable column using std::optional
    schema::column<"description", std::optional<std::string>> description;
    
    // Column with default value - automatically deduces type (double)
    schema::column<"price", double, schema::Default_value<0.0>> price;
    
    // Column with default value using a SQL literal
    schema::column<"created_at", std::string, schema::Default_value<schema::current_timestamp>> created_at;
    
    // Integer with default value - automatically deduces type (int)
    schema::column<"stock", int, schema::Default_value<0>> stock;
    
    // String with default value - automatically deduces type (string)
    schema::column<"status", std::string, schema::Default_value<"active">> status;
    
    // Float with default value - automatically deduces type (double)
    schema::column<"rate", double, schema::Default_value<1.5>> rate;
    
    // Boolean with default value - automatically deduces type (bool)
    schema::column<"is_featured", bool, schema::Default_value<false>> is_featured;
    
    // Nullable integer with DEFAULT NULL
    schema::column<"parent_id", std::optional<int>, schema::null_default> parent_id;
    
    // Table constraints
    schema::primary_key<&product::id> pk;
    schema::unique_constraint<&product::name> unique_name;
    schema::check_constraint<&product::price, "> 0"> valid_price;
    schema::check_constraint<&product::stock, ">= 0"> valid_stock;
    
    // Table-level check constraint
    schema::table_check_constraint<"status IN ('active', 'discontinued', 'out_of_stock')"> valid_status;
};

struct Order {
    schema::column<"id", int> id;
    schema::column<"product_id", int> product_id;
    
    // Integer with default value
    schema::column<"quantity", int, schema::Default_value<1>> quantity;
    
    schema::column<"user_id", std::optional<int>> user_id;
    
    // SQL literal default
    schema::column<"order_date", std::string, schema::Default_value<schema::current_timestamp>> order_date;
    
    // Boolean with automatically deduced type
    schema::column<"is_paid", bool, schema::Default_value<false>> is_paid;
    
    schema::primary_key<&order::id> pk;
    schema::foreign_key<&order::product_id, &product::id> product_fk;
    schema::check_constraint<&order::quantity, "> 0"> valid_quantity;
    
    // Composite unique constraint
    schema::composite_unique_constraint<&order::product_id, &order::user_id> unique_product_user;
};

} // namespace examples
} // namespace sqllib

int main() {
    // Create instances of the tables
    sqllib::examples::product product_table;
    sqllib::examples::order order_table;
    
    // Generate CREATE TABLE statements
    std::cout << "CREATE TABLE statement for products:\n";
    std::cout << sqllib::schema::create_table_sql(product_table) << "\n\n";
    
    std::cout << "CREATE TABLE statement for orders:\n";
    std::cout << sqllib::schema::create_table_sql(order_table) << "\n\n";
    
    // Test default values
    std::cout << "Default value for price: ";
    auto default_price = product_table.price.get_default_value();
    if (default_price) {
        std::cout << *default_price << "\n";
    } else {
        std::cout << "No default\n";
    }
    
    std::cout << "Default value for stock: ";
    auto default_stock = product_table.stock.get_default_value();
    if (default_stock) {
        std::cout << *default_stock << "\n";
    } else {
        std::cout << "No default\n";
    }
    
    std::cout << "Default value for rate: ";
    auto default_rate = product_table.rate.get_default_value();
    if (default_rate) {
        std::cout << *default_rate << "\n";
    } else {
        std::cout << "No default\n";
    }
    
    std::cout << "Default value for status: ";
    auto default_status = product_table.status.get_default_value();
    if (default_status) {
        std::cout << *default_status << "\n";
    } else {
        std::cout << "No default\n";
    }
    
    std::cout << "Default value for is_featured: ";
    auto default_featured = product_table.is_featured.get_default_value();
    if (default_featured) {
        std::cout << (*default_featured ? "true" : "false") << "\n";
    } else {
        std::cout << "No default\n";
    }
    
    std::cout << "Default value for description: ";
    auto default_desc = product_table.description.get_default_value();
    if (default_desc && default_desc->has_value()) {
        std::cout << **default_desc << "\n";
    } else {
        std::cout << "NULL\n";
    }
    
    std::cout << "Default value for parent_id: ";
    auto default_parent = product_table.parent_id.get_default_value();
    if (default_parent && default_parent->has_value()) {
        std::cout << **default_parent << "\n";
    } else {
        std::cout << "NULL\n";
    }
    
    return 0;
} 