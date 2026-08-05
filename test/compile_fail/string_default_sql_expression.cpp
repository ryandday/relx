// Must NOT compile: string_default<"CURRENT_TIMESTAMP"> would emit the quoted
// *string* 'CURRENT_TIMESTAMP', not the SQL expression - the static_assert directs
// to relx::default_sql instead.
#include <relx/schema.hpp>

// clang-format off: annotation syntax is not yet understood by clang-format 20

struct [[=relx::table("t")]] T {
  [[=relx::string_default<"CURRENT_TIMESTAMP">{}]] std::string created_at;
};

// clang-format on

inline constexpr auto t = relx::t<T>;

int main() {
  (void)relx::create_table_sql<T>().to_sql();
}
