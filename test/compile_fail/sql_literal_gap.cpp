// Must NOT compile: $n placeholders with a gap ($2 missing)
// expect-error: placeholders have a gap
#include <relx/sql_literal.hpp>

int main() {
  (void)relx::checked_sql<"SELECT $1, $3">();
}
