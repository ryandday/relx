// Must NOT compile: composite_pk conflicts with a column-level pk annotation
// expect-error: composite_pk conflicts with a column-level pk
#include <relx/schema.hpp>

struct[[= relx::table("t"), = relx::ann::composite_pk("a", "b")]] T {
  [[= relx::ann::pk]] int a;
  int b;
};

int main() {
  (void)relx::schema::table_constraints_sql<T>();
}
