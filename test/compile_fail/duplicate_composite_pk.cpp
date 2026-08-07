// Must NOT compile: a table can have at most one composite_pk
// expect-error: at most one composite_pk
#include <relx/schema.hpp>

struct[[= relx::table("t"), = relx::ann::composite_pk("a"), = relx::ann::composite_pk("b")]] T {
  int a;
  int b;
};

int main() {
  (void)relx::schema::table_constraints_sql<T>();
}
