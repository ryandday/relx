#pragma once

#include "core.hpp"
#include "column_expression.hpp"
#include "schema_adapter.hpp"
#include <memory>
#include <string>
#include <vector>
#include <sstream>

namespace relx {
namespace query {

/// @brief Generic binary condition expression
template <SqlExpr Left, SqlExpr Right>
class BinaryCondition : public SqlExpression {
public:
    BinaryCondition(Left left, std::string op, Right right)
        : left_(std::move(left)), op_(std::move(op)), right_(std::move(right)) {}

    std::string to_sql() const override {
        std::stringstream ss;
        ss << "(" << left_.to_sql() << " " << op_ << " " << right_.to_sql() << ")";
        return ss.str();
    }

    std::vector<std::string> bind_params() const override {
        auto left_params = left_.bind_params();
        auto right_params = right_.bind_params();
        left_params.insert(left_params.end(), right_params.begin(), right_params.end());
        return left_params;
    }

private:
    Left left_;
    std::string op_;
    Right right_;
};

/// @brief Equality condition (col = value)
template <SqlExpr Left, SqlExpr Right>
auto operator==(Left left, Right right) {
    return BinaryCondition<Left, Right>(std::move(left), "=", std::move(right));
}

/// @brief Inequality condition (col != value)
template <SqlExpr Left, SqlExpr Right>
auto operator!=(Left left, Right right) {
    return BinaryCondition<Left, Right>(std::move(left), "!=", std::move(right));
}

/// @brief Greater than condition (col > value)
template <SqlExpr Left, SqlExpr Right>
auto operator>(Left left, Right right) {
    return BinaryCondition<Left, Right>(std::move(left), ">", std::move(right));
}

/// @brief Less than condition (col < value)
template <SqlExpr Left, SqlExpr Right>
auto operator<(Left left, Right right) {
    return BinaryCondition<Left, Right>(std::move(left), "<", std::move(right));
}

/// @brief Greater than or equal condition (col >= value)
template <SqlExpr Left, SqlExpr Right>
auto operator>=(Left left, Right right) {
    return BinaryCondition<Left, Right>(std::move(left), ">=", std::move(right));
}

/// @brief Less than or equal condition (col <= value)
template <SqlExpr Left, SqlExpr Right>
auto operator<=(Left left, Right right) {
    return BinaryCondition<Left, Right>(std::move(left), "<=", std::move(right));
}

/// @brief Logical AND condition
template <SqlExpr Left, SqlExpr Right>
auto operator&&(Left left, Right right) {
    return BinaryCondition<Left, Right>(std::move(left), "AND", std::move(right));
}

/// @brief Logical OR condition
template <SqlExpr Left, SqlExpr Right>
auto operator||(Left left, Right right) {
    return BinaryCondition<Left, Right>(std::move(left), "OR", std::move(right));
}

/// @brief IN condition (col IN (values)) with type checking
template <SqlExpr Expr, std::ranges::range Range>
requires std::convertible_to<std::ranges::range_value_t<Range>, std::string>
class TypedInCondition : public SqlExpression {
public:
    TypedInCondition(Expr expr, Range values) 
        : expr_(std::move(expr)), values_(std::move(values)) {}

    std::string to_sql() const override {
        std::stringstream ss;
        ss << expr_.to_sql() << " IN (";
        bool first = true;
        for (const auto& _ : values_) {
            if (!first) ss << ", ";
            ss << "?";
            first = false;
        }
        ss << ")";
        return ss.str();
    }

    std::vector<std::string> bind_params() const override {
        auto params = expr_.bind_params();
        for (const auto& value : values_) {
            params.push_back(value);
        }
        return params;
    }

private:
    Expr expr_;
    Range values_;
};

/// @brief Create an IN condition with type checking for columns
/// @tparam Column The column type
/// @tparam Range The values range type
/// @param col The column
/// @param values The values to check against
/// @return An InCondition expression
template <typename TableT, schema::FixedString Name, typename T, typename... Modifiers, 
          std::ranges::range Range>
requires std::convertible_to<std::ranges::range_value_t<Range>, std::string>
auto in(const schema::column<TableT, Name, T, Modifiers...>& col, Range values) {
    using ValueType = std::ranges::range_value_t<Range>;
    
    // Type check for IN operations - ensure the values are compatible with the column type
    if constexpr (std::same_as<T, std::string> || std::same_as<T, std::string_view>) {
        static_assert(std::convertible_to<ValueType, std::string>,
                      "IN operation with string column requires string-convertible values");
    } else if constexpr (std::is_arithmetic_v<T>) {
        static_assert(std::convertible_to<ValueType, std::string>,
                      "IN operation values must be convertible to string for parameter binding");
    }
    
    auto col_expr = to_expr(col);
    return TypedInCondition<decltype(col_expr), Range>(std::move(col_expr), std::move(values));
}

/// @brief Original IN condition for backward compatibility
template <SqlExpr Expr, std::ranges::range Range>
requires std::convertible_to<std::ranges::range_value_t<Range>, std::string>
class InCondition : public SqlExpression {
public:
    InCondition(Expr expr, Range values) 
        : expr_(std::move(expr)), values_(std::move(values)) {}

    std::string to_sql() const override {
        std::stringstream ss;
        ss << expr_.to_sql() << " IN (";
        bool first = true;
        for (const auto& _ : values_) {
            if (!first) ss << ", ";
            ss << "?";
            first = false;
        }
        ss << ")";
        return ss.str();
    }

    std::vector<std::string> bind_params() const override {
        auto params = expr_.bind_params();
        for (const auto& value : values_) {
            params.push_back(value);
        }
        return params;
    }

private:
    Expr expr_;
    Range values_;
};

/// @brief Create an IN condition for expressions
/// @tparam Expr The expression type
/// @tparam Range The values range type
/// @param expr The column or expression
/// @param values The values to check against
/// @return An InCondition expression
template <SqlExpr Expr, std::ranges::range Range>
requires std::convertible_to<std::ranges::range_value_t<Range>, std::string>
auto in(Expr expr, Range values) {
    return InCondition<Expr, Range>(std::move(expr), std::move(values));
}

/// @brief LIKE condition (col LIKE pattern)
template <SqlExpr Expr>
class LikeCondition : public SqlExpression {
public:
    LikeCondition(Expr expr, std::string pattern)
        : expr_(std::move(expr)), pattern_(std::move(pattern)) {}

    std::string to_sql() const override {
        return expr_.to_sql() + " LIKE ?";
    }

    std::vector<std::string> bind_params() const override {
        auto params = expr_.bind_params();
        params.push_back(pattern_);
        return params;
    }

private:
    Expr expr_;
    std::string pattern_;
};

/// @brief Create a LIKE condition
/// @tparam Expr The expression type
/// @param expr The column or expression
/// @param pattern The LIKE pattern
/// @return A LikeCondition expression
template <SqlExpr Expr>
auto like(Expr expr, std::string pattern) {
    return LikeCondition<Expr>(std::move(expr), std::move(pattern));
}

/// @brief BETWEEN condition (col BETWEEN lower AND upper)
template <SqlExpr Expr>
class BetweenCondition : public SqlExpression {
public:
    BetweenCondition(Expr expr, std::string lower, std::string upper)
        : expr_(std::move(expr)), lower_(std::move(lower)), upper_(std::move(upper)) {}

    std::string to_sql() const override {
        return expr_.to_sql() + " BETWEEN ? AND ?";
    }

    std::vector<std::string> bind_params() const override {
        auto params = expr_.bind_params();
        params.push_back(lower_);
        params.push_back(upper_);
        return params;
    }

private:
    Expr expr_;
    std::string lower_;
    std::string upper_;
};

/// @brief Create a BETWEEN condition
/// @tparam Expr The expression type
/// @param expr The column or expression
/// @param lower The lower bound
/// @param upper The upper bound
/// @return A BetweenCondition expression
template <SqlExpr Expr>
auto between(Expr expr, std::string lower, std::string upper) {
    return BetweenCondition<Expr>(std::move(expr), std::move(lower), std::move(upper));
}

/// @brief IS NULL condition
template <SqlExpr Expr>
class IsNullCondition : public SqlExpression {
public:
    explicit IsNullCondition(Expr expr) : expr_(std::move(expr)) {}

    std::string to_sql() const override {
        return expr_.to_sql() + " IS NULL";
    }

    std::vector<std::string> bind_params() const override {
        return expr_.bind_params();
    }

private:
    Expr expr_;
};

/// @brief Create an IS NULL condition
/// @tparam Expr The expression type
/// @param expr The column or expression
/// @return An IsNullCondition expression
template <SqlExpr Expr>
auto is_null(Expr expr) {
    return IsNullCondition<Expr>(std::move(expr));
}

/// @brief IS NOT NULL condition
template <SqlExpr Expr>
class IsNotNullCondition : public SqlExpression {
public:
    explicit IsNotNullCondition(Expr expr) : expr_(std::move(expr)) {}

    std::string to_sql() const override {
        return expr_.to_sql() + " IS NOT NULL";
    }

    std::vector<std::string> bind_params() const override {
        return expr_.bind_params();
    }

private:
    Expr expr_;
};

/// @brief Create an IS NOT NULL condition
/// @tparam Expr The expression type
/// @param expr The column or expression
/// @return An IsNotNullCondition expression
template <SqlExpr Expr>
auto is_not_null(Expr expr) {
    return IsNotNullCondition<Expr>(std::move(expr));
}

/// @brief Negation condition (NOT expr)
template <SqlExpr Expr>
class NotCondition : public SqlExpression {
public:
    explicit NotCondition(Expr expr) : expr_(std::move(expr)) {}

    std::string to_sql() const override {
        return "(NOT " + expr_.to_sql() + ")";
    }

    std::vector<std::string> bind_params() const override {
        return expr_.bind_params();
    }

private:
    Expr expr_;
};

/// @brief Logical NOT operator
template <SqlExpr Expr>
auto operator!(Expr expr) {
    return NotCondition<Expr>(std::move(expr));
}

} // namespace query
} // namespace relx 