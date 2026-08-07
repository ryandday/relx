// Must NOT compile: a column-level annotation (pk) at struct level applies to nothing
// expect-error: column-level annotation at struct level
#include <relx/schema.hpp>

struct[[= relx::table("t"), = relx::ann::pk]] T {
  int x;
};

int main() {
  (void)relx::create_table_sql<T>().to_sql();
}
