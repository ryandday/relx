#pragma once

#include "core.hpp"
#include "column_expr.hpp"
#include <memory>
#include <string>
#include <vector>
#include <iostream>

namespace sqllib {
namespace query {

/// @brief Base class for SQL function expressions
/// @tparam Expr The expression type for the function argument
template <SqlExpr Expr>
class FunctionExpr : public ColumnExpression {
public:
    FunctionExpr(std::string name, Expr expr)
        : func_name_(std::move(name)), expr_(std::move(expr)) {}

    std::string to_sql() const override {
        return func_name_ + "(" + expr_.to_sql() + ")";
    }

    std::vector<std::string> bind_params() const override {
        return expr_.bind_params();
    }

    std::string column_name() const override {
        return func_name_ + "(" + expr_.column_name() + ")";
    }

    std::string table_name() const override {
        return "";
    }

private:
    std::string func_name_;
    Expr expr_;
};

/// @brief Base class for SQL function expressions with no arguments
class NullaryFunctionExpr : public ColumnExpression {
public:
    explicit NullaryFunctionExpr(std::string name)
        : func_name_(std::move(name)) {}

    std::string to_sql() const override {
        return func_name_ + "()";
    }

    std::vector<std::string> bind_params() const override {
        return {};
    }

    std::string column_name() const override {
        return func_name_ + "()";
    }

    std::string table_name() const override {
        return "";
    }

private:
    std::string func_name_;
};

/// @brief COUNT aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to count
/// @return A FunctionExpr representing COUNT(expr)
template <SqlExpr Expr>
auto count(Expr expr) {
    return FunctionExpr<Expr>("COUNT", std::move(expr));
}

/// @brief COUNT(*) aggregate function
/// @return A NullaryFunctionExpr representing COUNT(*)
inline auto count_all() {
    struct CountAllExpr : public ColumnExpression {
        std::string to_sql() const override {
            return "COUNT(*)";
        }

        std::vector<std::string> bind_params() const override {
            return {};
        }

        std::string column_name() const override {
            return "COUNT(*)";
        }

        std::string table_name() const override {
            return "";
        }
    };
    
    return CountAllExpr{};
}

/// @brief SUM aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to sum
/// @return A FunctionExpr representing SUM(expr)
template <SqlExpr Expr>
auto sum(Expr expr) {
    return FunctionExpr<Expr>("SUM", std::move(expr));
}

/// @brief AVG aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to average
/// @return A FunctionExpr representing AVG(expr)
template <SqlExpr Expr>
auto avg(Expr expr) {
    return FunctionExpr<Expr>("AVG", std::move(expr));
}

/// @brief MIN aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to find minimum of
/// @return A FunctionExpr representing MIN(expr)
template <SqlExpr Expr>
auto min(Expr expr) {
    return FunctionExpr<Expr>("MIN", std::move(expr));
}

/// @brief MAX aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to find maximum of
/// @return A FunctionExpr representing MAX(expr)
template <SqlExpr Expr>
auto max(Expr expr) {
    return FunctionExpr<Expr>("MAX", std::move(expr));
}

/// @brief DISTINCT qualifier for an expression
/// @tparam Expr The expression type
/// @param expr The expression to apply DISTINCT to
/// @return A SqlExpression representing DISTINCT expr
template <SqlExpr Expr>
class DistinctExpr : public ColumnExpression {
public:
    explicit DistinctExpr(Expr expr) : expr_(std::move(expr)) {}

    std::string to_sql() const override {
        return "DISTINCT " + expr_.to_sql();
    }

    std::vector<std::string> bind_params() const override {
        return expr_.bind_params();
    }

    std::string column_name() const override {
        if constexpr (std::is_base_of_v<ColumnExpression, Expr>) {
            return "DISTINCT_" + expr_.column_name();
        } else {
            return "DISTINCT_EXPR";
        }
    }

    std::string table_name() const override {
        if constexpr (std::is_base_of_v<ColumnExpression, Expr>) {
            return expr_.table_name();
        } else {
            return "";
        }
    }

private:
    Expr expr_;
};

/// @brief Create a DISTINCT expression
/// @tparam Expr The expression type
/// @param expr The expression to apply DISTINCT to
/// @return A DistinctExpr
template <SqlExpr Expr>
auto distinct(Expr expr) {
    return DistinctExpr<Expr>(std::move(expr));
}

/// @brief COUNT(DISTINCT expr) aggregate function
/// @tparam Expr The expression type
/// @param expr The expression to count distinct values of
/// @return A FunctionExpr representing COUNT(DISTINCT expr)
template <SqlExpr Expr>
auto count_distinct(Expr expr) {
    return count(distinct(std::move(expr)));
}

/// @brief LOWER string function
/// @tparam Expr The expression type
/// @param expr The string expression to convert to lowercase
/// @return A FunctionExpr representing LOWER(expr)
template <SqlExpr Expr>
auto lower(Expr expr) {
    return FunctionExpr<Expr>("LOWER", std::move(expr));
}

/// @brief UPPER string function
/// @tparam Expr The expression type
/// @param expr The string expression to convert to uppercase
/// @return A FunctionExpr representing UPPER(expr)
template <SqlExpr Expr>
auto upper(Expr expr) {
    return FunctionExpr<Expr>("UPPER", std::move(expr));
}

/// @brief LENGTH string function
/// @tparam Expr The expression type
/// @param expr The string expression to get length of
/// @return A FunctionExpr representing LENGTH(expr)
template <SqlExpr Expr>
auto length(Expr expr) {
    return FunctionExpr<Expr>("LENGTH", std::move(expr));
}

/// @brief TRIM string function
/// @tparam Expr The expression type
/// @param expr The string expression to trim
/// @return A FunctionExpr representing TRIM(expr)
template <SqlExpr Expr>
auto trim(Expr expr) {
    return FunctionExpr<Expr>("TRIM", std::move(expr));
}

/// @brief COALESCE function
/// @tparam First The first expression type
/// @tparam Second The second expression type
/// @tparam Rest The types of the remaining expressions
/// @param first The first expression
/// @param second The second expression
/// @param rest The remaining expressions
/// @return A SqlExpression representing COALESCE(first, second, ...)
template <SqlExpr First, SqlExpr Second, SqlExpr... Rest>
class CoalesceExpr : public ColumnExpression {
public:
    CoalesceExpr(First first, Second second, Rest... rest)
        : first_(std::move(first)), second_(std::move(second)), rest_(std::make_tuple(std::move(rest)...)) {}

    std::string to_sql() const override {
        std::stringstream ss;
        ss << "COALESCE(" << first_.to_sql() << ", " << second_.to_sql();
        
        std::apply([&](const auto&... items) {
            ((ss << ", " << items.to_sql()), ...);
        }, rest_);
        
        ss << ")";
        return ss.str();
    }

    std::vector<std::string> bind_params() const override {
        std::vector<std::string> params;
        
        auto first_params = first_.bind_params();
        params.insert(params.end(), first_params.begin(), first_params.end());
        
        auto second_params = second_.bind_params();
        params.insert(params.end(), second_params.begin(), second_params.end());
        
        std::apply([&](const auto&... items) {
            ((params.insert(params.end(), 
                            items.bind_params().begin(), 
                            items.bind_params().end())), ...);
        }, rest_);
        
        return params;
    }

    std::string column_name() const override {
        return "COALESCE";
    }

    std::string table_name() const override {
        return "";
    }

private:
    First first_;
    Second second_;
    std::tuple<Rest...> rest_;
};

/// @brief Create a COALESCE expression
/// @tparam First The first expression type
/// @tparam Second The second expression type
/// @tparam Rest The types of the remaining expressions
/// @param first The first expression
/// @param second The second expression
/// @param rest The remaining expressions
/// @return A CoalesceExpr
template <SqlExpr First, SqlExpr Second, SqlExpr... Rest>
auto coalesce(First first, Second second, Rest... rest) {
    return CoalesceExpr<First, Second, Rest...>(
        std::move(first), std::move(second), std::move(rest)...
    );
}

// First define CaseExpr class fully
class CaseExpr : public ColumnExpression {
public:
    using WhenThenPair = std::pair<std::unique_ptr<SqlExpression>, std::unique_ptr<SqlExpression>>;
    
    explicit CaseExpr(std::vector<WhenThenPair>&& when_thens, std::unique_ptr<SqlExpression> else_expr)
        : when_thens_(std::move(when_thens)), else_expr_(std::move(else_expr)) {}
    
    // Allow move operations
    CaseExpr(CaseExpr&&) = default;
    CaseExpr& operator=(CaseExpr&&) = default;
    
    // Disable copy operations (due to unique_ptr)
    CaseExpr(const CaseExpr&) = delete;
    CaseExpr& operator=(const CaseExpr&) = delete;
    
    std::string to_sql() const override {
        std::string sql = "CASE";
        
        for (const auto& [when_cond, then_val] : when_thens_) {
            sql += " WHEN (";
            std::string cond = when_cond->to_sql();
            // Remove any existing outer parentheses to avoid double parentheses
            if (cond.size() >= 2 && cond.front() == '(' && cond.back() == ')') {
                cond = cond.substr(1, cond.size() - 2);
            }
            sql += cond + ") THEN " + then_val->to_sql();
        }
        
        if (else_expr_) {
            sql += " ELSE " + else_expr_->to_sql();
        }
        
        sql += " END";
        return sql;
    }
    
    std::vector<std::string> bind_params() const override {
        std::vector<std::string> params;
        
        // Interleave condition and value parameters in the expected order
        for (const auto& [when_cond, then_val] : when_thens_) {
            // First condition parameters
            auto when_params = when_cond->bind_params();
            params.insert(params.end(), when_params.begin(), when_params.end());
            
            // Then the value parameters
            auto then_params = then_val->bind_params();
            params.insert(params.end(), then_params.begin(), then_params.end());
        }
        
        // Finally collect ELSE parameters if present
        if (else_expr_) {
            auto else_params = else_expr_->bind_params();
            params.insert(params.end(), else_params.begin(), else_params.end());
        }
        
        return params;
    }

    std::string column_name() const override {
        return "CASE";
    }

    std::string table_name() const override {
        return "";
    }

private:
    std::vector<WhenThenPair> when_thens_;
    std::unique_ptr<SqlExpression> else_expr_;
};

// Then define CaseBuilder that uses CaseExpr
class CaseBuilder {
public:
    CaseBuilder() : when_thens_(), else_expr_(nullptr) {}
    
    template <SqlExpr Then>
    CaseBuilder& when(const ConditionExpr auto& when, const Then& then) {
        when_thens_.emplace_back(
            std::make_unique<std::remove_cvref_t<decltype(when)>>(when),
            std::make_unique<Then>(then)
        );
        return *this;
    }
    
    auto build() {
        return CaseExpr(std::move(when_thens_), std::move(else_expr_));
    }
    
    template <SqlExpr Else>
    CaseBuilder& else_(const Else& else_expr) {
        else_expr_ = std::make_unique<Else>(else_expr);
        return *this;
    }

private:
    using WhenThenPair = std::pair<std::unique_ptr<SqlExpression>, std::unique_ptr<SqlExpression>>;
    std::vector<WhenThenPair> when_thens_;
    std::unique_ptr<SqlExpression> else_expr_;
};

/// @brief Create a CASE expression
/// @return A CaseBuilder
inline auto case_() {
    return CaseBuilder();
}

// Specialized as function for CaseExpr
inline auto as(CaseExpr&& expr, std::string alias) {
    auto expr_ptr = std::make_shared<CaseExpr>(std::move(expr));
    return AliasedColumn<CaseExpr>(std::move(expr_ptr), std::move(alias));
}

} // namespace query
} // namespace sqllib 