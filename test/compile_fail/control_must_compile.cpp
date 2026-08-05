// Control: a valid annotated table MUST compile - if this fails, the compile-fail
// tests are failing for the wrong reason (flag rot, header breakage)
#include <relx/schema.hpp>

struct[[= relx::table("parent"), = relx::ann::composite_pk("a", "b")]] Parent {
  int a;
  int b;
};

struct[[
  = relx::table("child"), = relx::ann::composite_fk<^^Parent::a, ^^Parent::b>("pa", "pb"),
  = relx::ann::index_on("pa").unique(), = relx::ann::check("pa > 0")
]] Child {
  int pa;
  int pb;
};

int main() {
  (void)relx::schema::table_constraints_sql<Child>();
  (void)relx::create_indexes_sql<Child>();
}
