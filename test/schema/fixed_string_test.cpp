#include <gtest/gtest.h>
#include <relx/schema/fixed_string.hpp>
#include <sstream>

using namespace relx::schema;
// Explicitly bring in the literals namespace for the _fs operator
using namespace relx::literals;

TEST(fixed_stringTest, Construction) {
    // Test basic construction from string literal
    constexpr fixed_string<6> str1("hello");
    EXPECT_EQ(std::string_view(str1), "hello");
    
    // Test construction with empty string
    constexpr fixed_string<1> empty("");
    EXPECT_EQ(std::string_view(empty), "");
    EXPECT_TRUE(empty.empty());
    
    // Test copy construction
    constexpr fixed_string<6> str2 = str1;
    EXPECT_EQ(std::string_view(str2), "hello");
}

TEST(fixed_stringTest, Size) {
    constexpr fixed_string<6> str("hello");
    EXPECT_EQ(str.size(), 5); // size excludes null terminator
    
    constexpr fixed_string<1> empty("");
    EXPECT_EQ(empty.size(), 0);
}

TEST(fixed_stringTest, Comparison) {
    constexpr fixed_string<6> hello1("hello");
    constexpr fixed_string<6> hello2("hello");
    constexpr fixed_string<6> world("world");
    
    EXPECT_TRUE(hello1 == hello2);
    EXPECT_FALSE(hello1 == world);
    EXPECT_TRUE(hello1 != world);
    EXPECT_FALSE(hello1 != hello2);
    
    // Different size but same content
    constexpr fixed_string<7> hello_padded("hello ");
    EXPECT_FALSE(hello1 == hello_padded);
    EXPECT_TRUE(hello1 != hello_padded);
}

TEST(fixed_stringTest, StringViewConversion) {
    constexpr fixed_string<6> str("hello");
    std::string_view sv = str;
    
    EXPECT_EQ(sv, "hello");
    EXPECT_EQ(sv.size(), 5);
}

TEST(fixed_stringTest, CStrAccess) {
    constexpr fixed_string<6> str("hello");
    EXPECT_STREQ(str.c_str(), "hello");
}

TEST(fixed_stringTest, StreamOutput) {
    fixed_string<6> str("hello");
    std::ostringstream oss;
    oss << str;
    EXPECT_EQ(oss.str(), "hello");
}

// Test for user-defined literal (using constructor directly)
TEST(fixed_stringTest, UserDefinedLiteralEquivalent) {
    constexpr fixed_string<6> str("hello");
    EXPECT_EQ(std::string_view(str), "hello");
} 