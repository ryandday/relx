#include <gtest/gtest.h>
#include <relx/schema.hpp>

using namespace relx::schema;

// Test table with various index types
struct Product {
    static constexpr auto table_name = "products";
    
    column<Product, "id", int> id;
    column<Product, "name", std::string> name_col;
    column<Product, "sku", std::string> sku;
    column<Product, "description", std::string> description;
    column<Product, "price", double> price;
    column<Product, "stock", int> stock;
    
    // Primary key
    table_primary_key<&Product::id> pk;
    
    // Various index types
    relx::schema::index<&Product::sku> sku_idx{index_type::unique};
    relx::schema::index<&Product::name_col> name_idx{index_type::normal};
    relx::schema::index<&Product::description> desc_idx{index_type::fulltext};
    
    // Composite index (may not be fully implemented yet)
    composite_index<&Product::price, &Product::stock> price_stock_idx;
};

TEST(IndexTest, IndexTypes) {
    // Test conversion of index types to SQL strings
    EXPECT_EQ(std::string_view(index_type_to_string(index_type::normal)), "");
    EXPECT_EQ(std::string_view(index_type_to_string(index_type::unique)), "UNIQUE ");
    EXPECT_EQ(std::string_view(index_type_to_string(index_type::fulltext)), "FULLTEXT ");
    EXPECT_EQ(std::string_view(index_type_to_string(index_type::spatial)), "SPATIAL ");
}

TEST(IndexTest, NormalIndex) {
    Product product;
    
    // Test the CREATE INDEX statement for a normal index
    const std::string expected = "CREATE INDEX products_name_idx ON products (name)";
    EXPECT_EQ(product.name_idx.create_index_sql(), expected);
}

TEST(IndexTest, UniqueIndex) {
    Product product;
    
    // Test the CREATE INDEX statement for a unique index
    const std::string expected = "CREATE UNIQUE INDEX products_sku_idx ON products (sku)";
    EXPECT_EQ(product.sku_idx.create_index_sql(), expected);
}

TEST(IndexTest, FulltextIndex) {
    Product product;
    
    // Test the CREATE INDEX statement for a fulltext index
    const std::string expected = "CREATE FULLTEXT INDEX products_description_idx ON products (description)";
    EXPECT_EQ(product.desc_idx.create_index_sql(), expected);
}

TEST(IndexTest, CompositeIndex) {
    Product product;
    
    // Note: This test checks the intended behavior of composite indexes
    // but the implementation may not support them yet
    
    // Expected behavior for composite index 
    // Actual implementation might differ
    const std::string expected = "CREATE INDEX products_price_stock_idx ON products (price, stock)";
    EXPECT_EQ(product.price_stock_idx.create_index_sql(), expected);
}

TEST(IndexTest, DefaultIndexConstructor) {
    // Test that the default constructor creates a normal index
    relx::schema::index<&Product::name_col> default_idx;
    Product product;
    
    // The expected CREATE INDEX statement for a normal index
    const std::string expected = "CREATE INDEX products_name_idx ON products (name)";
    EXPECT_EQ(default_idx.create_index_sql(), expected);
}

TEST(IndexTest, ExplicitlyNormalIndex) {
    // Test creating a normal index explicitly
    relx::schema::index<&Product::name_col> normal_idx{index_type::normal};
    Product product;
    
    // The expected CREATE INDEX statement for a normal index
    const std::string expected = "CREATE INDEX products_name_idx ON products (name)";
    EXPECT_EQ(normal_idx.create_index_sql(), expected);
}

TEST(IndexTest, SpatialIndex) {
    // Test creating a spatial index
    // Note: This would typically be used for geometry/geography columns
    relx::schema::index<&Product::name_col> spatial_idx{index_type::spatial};
    Product product;
    
    // The expected CREATE INDEX statement for a spatial index
    const std::string expected = "CREATE SPATIAL INDEX products_name_idx ON products (name)";
    EXPECT_EQ(spatial_idx.create_index_sql(), expected);
} 