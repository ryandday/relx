// Must NOT compile: declared parameter count does not match the placeholders
// expect-error: placeholder count does not match
#include <relx/sql_literal.hpp>

int main() {
  (void)relx::checked_sql<"SELECT $1, $2", 3>();
}
