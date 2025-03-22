#include <gtest/gtest.h>
#include <sqllib/schema/fixed_string.hpp>
#include <sstream>

using namespace sqllib::schema;
// Explicitly bring in the literals namespace for the _fs operator
using namespace sqllib::literals;

TEST(FixedStringTest, Construction) {
    // Test basic construction from string literal
    constexpr FixedString<6> str1("hello");
    EXPECT_EQ(std::string_view(str1), "hello");
    
    // Test construction with empty string
    constexpr FixedString<1> empty("");
    EXPECT_EQ(std::string_view(empty), "");
    EXPECT_TRUE(empty.empty());
    
    // Test copy construction
    constexpr FixedString<6> str2 = str1;
    EXPECT_EQ(std::string_view(str2), "hello");
}

TEST(FixedStringTest, Size) {
    constexpr FixedString<6> str("hello");
    EXPECT_EQ(str.size(), 5); // size excludes null terminator
    
    constexpr FixedString<1> empty("");
    EXPECT_EQ(empty.size(), 0);
}

TEST(FixedStringTest, Comparison) {
    constexpr FixedString<6> hello1("hello");
    constexpr FixedString<6> hello2("hello");
    constexpr FixedString<6> world("world");
    
    EXPECT_TRUE(hello1 == hello2);
    EXPECT_FALSE(hello1 == world);
    EXPECT_TRUE(hello1 != world);
    EXPECT_FALSE(hello1 != hello2);
    
    // Different size but same content
    constexpr FixedString<7> hello_padded("hello ");
    EXPECT_FALSE(hello1 == hello_padded);
    EXPECT_TRUE(hello1 != hello_padded);
}

TEST(FixedStringTest, StringViewConversion) {
    constexpr FixedString<6> str("hello");
    std::string_view sv = str;
    
    EXPECT_EQ(sv, "hello");
    EXPECT_EQ(sv.size(), 5);
}

TEST(FixedStringTest, CStrAccess) {
    constexpr FixedString<6> str("hello");
    EXPECT_STREQ(str.c_str(), "hello");
}

TEST(FixedStringTest, StreamOutput) {
    FixedString<6> str("hello");
    std::ostringstream oss;
    oss << str;
    EXPECT_EQ(oss.str(), "hello");
}

// Test for user-defined literal (using constructor directly)
TEST(FixedStringTest, UserDefinedLiteralEquivalent) {
    constexpr FixedString<6> str("hello");
    EXPECT_EQ(std::string_view(str), "hello");
} 