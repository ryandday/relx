// Must NOT compile: $n placeholders with a gap ($2 missing)
#include <relx/sql_literal.hpp>

int main() {
  (void)relx::checked_sql<"SELECT $1, $3">();
}
