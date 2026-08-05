#include "relx/connection/sql_utils.hpp"

#include <gtest/gtest.h>

namespace relx::connection {

class PostgreSQLPlaceholderTest : public ::testing::Test {
protected:
  // Helper function to test the convert_placeholders function
  std::string convert_placeholders(const std::string& sql) {
    return sql_utils::convert_placeholders_to_postgresql(sql);
  }
};

TEST_F(PostgreSQLPlaceholderTest, BasicPlaceholderReplacement) {
  EXPECT_EQ("SELECT * FROM users WHERE id = $1",
            convert_placeholders("SELECT * FROM users WHERE id = ?"));

  EXPECT_EQ("SELECT * FROM users WHERE id = $1 AND name = $2",
            convert_placeholders("SELECT * FROM users WHERE id = ? AND name = ?"));
}

TEST_F(PostgreSQLPlaceholderTest, NoPlaceholders) {
  EXPECT_EQ("SELECT * FROM users", convert_placeholders("SELECT * FROM users"));
}

TEST_F(PostgreSQLPlaceholderTest, QuestionMarkInStringLiteral) {
  // This test demonstrates the current problem - question marks in string literals
  // should NOT be replaced with parameter placeholders

  // Single quotes
  EXPECT_EQ("SELECT * FROM users WHERE name = 'What?' AND id = $1",
            convert_placeholders("SELECT * FROM users WHERE name = 'What?' AND id = ?"));

  // Double quotes (identifier)
  EXPECT_EQ("SELECT \"weird?column\" FROM users WHERE id = $1",
            convert_placeholders("SELECT \"weird?column\" FROM users WHERE id = ?"));

  // Multiple string literals
  EXPECT_EQ("SELECT * FROM users WHERE question = 'Why?' AND answer = 'Because!' AND id = $1",
            convert_placeholders(
                "SELECT * FROM users WHERE question = 'Why?' AND answer = 'Because!' AND id = ?"));
}

TEST_F(PostgreSQLPlaceholderTest, EscapedQuotesInStringLiteral) {
  // Test escaped quotes within string literals
  EXPECT_EQ(
      "SELECT * FROM users WHERE name = 'John''s question?' AND id = $1",
      convert_placeholders("SELECT * FROM users WHERE name = 'John''s question?' AND id = ?"));

  EXPECT_EQ("SELECT * FROM users WHERE name = 'Say \"What?\"' AND id = $1",
            convert_placeholders("SELECT * FROM users WHERE name = 'Say \"What?\"' AND id = ?"));
}

TEST_F(PostgreSQLPlaceholderTest, MultilineStrings) {
  std::string sql = R"(
        SELECT * FROM users 
        WHERE description = 'This is a long description.
        Does it work? I hope so!' 
        AND id = ?
    )";

  std::string expected = R"(
        SELECT * FROM users 
        WHERE description = 'This is a long description.
        Does it work? I hope so!' 
        AND id = $1
    )";

  EXPECT_EQ(expected, convert_placeholders(sql));
}

TEST_F(PostgreSQLPlaceholderTest, ComplexMixedCase) {
  // Complex case with multiple types of quotes and placeholders
  std::string sql =
      "SELECT \"table?name\", 'string?literal', ? FROM users WHERE id = ? AND name = 'What''s up?'";
  std::string expected = "SELECT \"table?name\", 'string?literal', $1 FROM users WHERE id = $2 AND "
                         "name = 'What''s up?'";

  EXPECT_EQ(expected, convert_placeholders(sql));
}

TEST_F(PostgreSQLPlaceholderTest, EmptyString) {
  EXPECT_EQ("", convert_placeholders(""));
}

TEST_F(PostgreSQLPlaceholderTest, OnlyQuestionMark) {
  EXPECT_EQ("$1", convert_placeholders("?"));
}

TEST_F(PostgreSQLPlaceholderTest, OnlyStringLiteral) {
  EXPECT_EQ("'test?string'", convert_placeholders("'test?string'"));
  EXPECT_EQ("\"test?identifier\"", convert_placeholders("\"test?identifier\""));
}

TEST_F(PostgreSQLPlaceholderTest, EscapedQuotesEdgeCases) {
  // Test consecutive escaped quotes
  EXPECT_EQ("'Don''t ask ''why?'' twice'", convert_placeholders("'Don''t ask ''why?'' twice'"));

  EXPECT_EQ("\"column\"\"with\"\"question?\"",
            convert_placeholders("\"column\"\"with\"\"question?\""));
}

TEST_F(PostgreSQLPlaceholderTest, UnmatchedQuotes) {
  // Test that unmatched quotes are handled gracefully
  // If a string starts but doesn't end, everything after becomes part of the string
  EXPECT_EQ("SELECT 'unclosed string with ? mark",
            convert_placeholders("SELECT 'unclosed string with ? mark"));

  EXPECT_EQ("SELECT \"unclosed identifier with ? mark",
            convert_placeholders("SELECT \"unclosed identifier with ? mark"));
}

TEST_F(PostgreSQLPlaceholderTest, NestedQuotes) {
  // Test mixing single and double quotes
  EXPECT_EQ("SELECT 'text with \"nested?quotes\"' AND column = $1",
            convert_placeholders("SELECT 'text with \"nested?quotes\"' AND column = ?"));

  EXPECT_EQ("SELECT \"identifier with 'nested?quotes'\" AND column = $1",
            convert_placeholders("SELECT \"identifier with 'nested?quotes'\" AND column = ?"));
}

TEST_F(PostgreSQLPlaceholderTest, ManyParameters) {
  std::string sql = "INSERT INTO test VALUES (?, ?, ?, ?, ?)";
  std::string expected = "INSERT INTO test VALUES ($1, $2, $3, $4, $5)";

  EXPECT_EQ(expected, convert_placeholders(sql));
}

TEST_F(PostgreSQLPlaceholderTest, RealWorldExample) {
  std::string sql = R"(
        SELECT u.name, u.email, 'Question: What''s your favorite color?' as prompt
        FROM "user_table?" u
        WHERE u.active = ? 
        AND u.name LIKE 'John?%' 
        AND u.created_at > ?
        ORDER BY u.name
    )";

  std::string expected = R"(
        SELECT u.name, u.email, 'Question: What''s your favorite color?' as prompt
        FROM "user_table?" u
        WHERE u.active = $1 
        AND u.name LIKE 'John?%' 
        AND u.created_at > $2
        ORDER BY u.name
    )";

  EXPECT_EQ(expected, convert_placeholders(sql));
}

}  // namespace relx::connection
namespace relx::connection {

TEST_F(PostgreSQLPlaceholderTest, LineCommentsAreOpaque) {
  EXPECT_EQ("SELECT $1 -- is this a param? no\n FROM t WHERE x = $2",
            convert_placeholders("SELECT ? -- is this a param? no\n FROM t WHERE x = ?"));
}

TEST_F(PostgreSQLPlaceholderTest, BlockCommentsAreOpaqueAndNest) {
  EXPECT_EQ("SELECT $1 /* what? /* nested? */ still comment? */ , $2",
            convert_placeholders("SELECT ? /* what? /* nested? */ still comment? */ , ?"));
}

TEST_F(PostgreSQLPlaceholderTest, DollarQuotedBodiesAreOpaque) {
  EXPECT_EQ("SELECT $$is this? no$$, $1", convert_placeholders("SELECT $$is this? no$$, ?"));
  EXPECT_EQ("SELECT $fn$body? with $$ inside$fn$, $1",
            convert_placeholders("SELECT $fn$body? with $$ inside$fn$, ?"));
}

TEST_F(PostgreSQLPlaceholderTest, EscapeStringsHonorBackslashes) {
  // The \' does not close the E-string, so the ? inside stays literal
  EXPECT_EQ("SELECT E'it\\'s?', $1", convert_placeholders("SELECT E'it\\'s?', ?"));
}

TEST_F(PostgreSQLPlaceholderTest, DoubledQuestionMarkIsLiteral) {
  // ?? escapes a literal ? (the JSONB operator convention)
  EXPECT_EQ("SELECT data ? 'key' FROM t WHERE id = $1",
            convert_placeholders("SELECT data ?? 'key' FROM t WHERE id = ?"));
}

}  // namespace relx::connection
