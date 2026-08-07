// Must NOT compile: composite_pk names a column that does not exist
// expect-error: is not a column of this table
#include <relx/schema.hpp>

struct[[= relx::table("t"), = relx::ann::composite_pk("a", "nope")]] T {
  int a;
  int b;
};

int main() {
  (void)relx::schema::table_constraints_sql<T>();
}
