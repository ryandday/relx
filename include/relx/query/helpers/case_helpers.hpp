#pragma once

#include "../function.hpp"
#include "../value.hpp"

namespace relx::query {

// Extension methods for CaseBuilder to work with literals in THEN clauses

// For string literals
inline CaseBuilder& when(CaseBuilder& builder, const ConditionExpr auto& condition,
                         const char* value) {
  return builder.when(condition, val(value));
}

// For string
inline CaseBuilder& when(CaseBuilder& builder, const ConditionExpr auto& condition,
                         const std::string& value) {
  return builder.when(condition, val(value));
}

// For integers
inline CaseBuilder& when(CaseBuilder& builder, const ConditionExpr auto& condition, int value) {
  return builder.when(condition, val(value));
}

// For long integers
inline CaseBuilder& when(CaseBuilder& builder, const ConditionExpr auto& condition, long value) {
  return builder.when(condition, val(value));
}

// For long long integers
inline CaseBuilder& when(CaseBuilder& builder, const ConditionExpr auto& condition,
                         long long value) {
  return builder.when(condition, val(value));
}

// For double
inline CaseBuilder& when(CaseBuilder& builder, const ConditionExpr auto& condition, double value) {
  return builder.when(condition, val(value));
}

// For float
inline CaseBuilder& when(CaseBuilder& builder, const ConditionExpr auto& condition, float value) {
  return builder.when(condition, val(value));
}

// For bool
inline CaseBuilder& when(CaseBuilder& builder, const ConditionExpr auto& condition, bool value) {
  return builder.when(condition, val(value));
}

// Helper for else_ with literals

// For string literals
// NOLINTNEXTLINE(readability-identifier-naming)
inline CaseBuilder& else_(CaseBuilder& builder, const char* value) {
  return builder.else_(val(value));
}

// For string
// NOLINTNEXTLINE(readability-identifier-naming)
inline CaseBuilder& else_(CaseBuilder& builder, const std::string& value) {
  return builder.else_(val(value));
}

// For integers
// NOLINTNEXTLINE(readability-identifier-naming)
inline CaseBuilder& else_(CaseBuilder& builder, int value) {
  return builder.else_(val(value));
}

// For long integers
// NOLINTNEXTLINE(readability-identifier-naming)
inline CaseBuilder& else_(CaseBuilder& builder, long value) {
  return builder.else_(val(value));
}

// For long long integers
// NOLINTNEXTLINE(readability-identifier-naming)
inline CaseBuilder& else_(CaseBuilder& builder, long long value) {
  return builder.else_(val(value));
}

// For double
// NOLINTNEXTLINE(readability-identifier-naming)
inline CaseBuilder& else_(CaseBuilder& builder, double value) {
  return builder.else_(val(value));
}

// For float
// NOLINTNEXTLINE(readability-identifier-naming)
inline CaseBuilder& else_(CaseBuilder& builder, float value) {
  return builder.else_(val(value));
}

// For bool
// NOLINTNEXTLINE(readability-identifier-naming)
inline CaseBuilder& else_(CaseBuilder& builder, bool value) {
  return builder.else_(val(value));
}

}  // namespace relx::query