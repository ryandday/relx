// Must NOT compile: a table-level annotation (check) on a member silently never
// reaches the database
// expect-error: not a column modifier
#include <relx/schema.hpp>

struct[[= relx::table("t")]] T {
  [[= relx::ann::check("x > 0")]] int x;
};

int main() {
  (void)relx::create_table_sql<T>().to_sql();
}
