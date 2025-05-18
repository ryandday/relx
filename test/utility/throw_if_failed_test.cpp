#include <gtest/gtest.h>
#include <relx/utils/error_handling.hpp>

TEST(ExpectedUtilityTest, ValueOrThrowReturnsValue) {
    // Create a successful result with integer value
    std::expected<int, relx::connection::ConnectionError> success_result{42};
    
    // Ensure the value is returned without throwing
    EXPECT_EQ(relx::value_or_throw(success_result), 42);
}

TEST(ExpectedUtilityTest, ValueOrThrowCausesException) {
    // Create an error result
    relx::connection::ConnectionError error{
        .message = "Test connection error",
        .error_code = 123
    };
    
    auto result = std::unexpected<relx::connection::ConnectionError>(error);
    std::expected<int, relx::connection::ConnectionError> error_result = result;
    
    // Test that an exception is thrown
    EXPECT_THROW({
        relx::value_or_throw(error_result);
    }, relx::RelxException);
    
    // Test with context
    EXPECT_THROW({
        relx::value_or_throw(error_result, "Custom context");
    }, relx::RelxException);
}

TEST(ExpectedUtilityTest, ValueOrThrowQueryErrorFormatted) {
    relx::query::QueryError query_error{.message = "SQL syntax error"};
    std::expected<int, relx::query::QueryError> result = 
        std::unexpected<relx::query::QueryError>(query_error);
    
    EXPECT_THROW({
        relx::value_or_throw(result);
    }, relx::RelxException);
}

TEST(ExpectedUtilityTest, ValueOrThrowResultErrorFormatted) {
    relx::result::ResultError result_error{.message = "Type conversion failed"};
    std::expected<int, relx::result::ResultError> result = 
        std::unexpected<relx::result::ResultError>(result_error);
    
    EXPECT_THROW({
        relx::value_or_throw(result);
    }, relx::RelxException);
}

TEST(ExpectedUtilityTest, ValueOrThrowRvalueSupport) {
    // Test with rvalue success
    EXPECT_EQ(
        relx::value_or_throw(std::expected<int, relx::connection::ConnectionError>{123}), 
        123
    );
    
    // Test with rvalue error
    EXPECT_THROW({
        relx::value_or_throw(std::expected<int, relx::connection::ConnectionError>{
            std::unexpected<relx::connection::ConnectionError>(
                relx::connection::ConnectionError{
                    .message = "Connection failed",
                    .error_code = 500
                }
            )
        });
    }, relx::RelxException);
}

TEST(ExpectedUtilityTest, ThrowIfFailedVoidResultType) {
    // Test successful void result
    std::expected<void, relx::connection::ConnectionError> success_void{};
    
    // Should not throw for success case
    EXPECT_NO_THROW({
        relx::throw_if_failed(success_void);
    });
    
    // Test with error
    relx::connection::ConnectionError error{
        .message = "Void operation failed",
        .error_code = 999
    };
    
    std::expected<void, relx::connection::ConnectionError> error_void = 
        std::unexpected<relx::connection::ConnectionError>(error);
    
    // Should throw for error case
    EXPECT_THROW({
        relx::throw_if_failed(error_void);
    }, relx::RelxException);
    
    // Test rvalue version
    EXPECT_NO_THROW({
        relx::throw_if_failed(std::expected<void, relx::connection::ConnectionError>{});
    });
    
    // Test rvalue error version
    EXPECT_THROW({
        relx::throw_if_failed(std::expected<void, relx::connection::ConnectionError>{
            std::unexpected<relx::connection::ConnectionError>(
                relx::connection::ConnectionError{
                    .message = "Rvalue void operation failed",
                    .error_code = 888
                }
            )
        });
    }, relx::RelxException);
} 