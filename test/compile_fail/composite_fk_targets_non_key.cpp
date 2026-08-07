// Must NOT compile: composite_fk must reference the target's primary key or a
// declared unique set
// expect-error: composite_fk must reference the target table
#include <relx/schema.hpp>

struct[[= relx::table("parent"), = relx::ann::composite_pk("a", "b")]] Parent {
  int a;
  int b;
  int c;
};

struct[[= relx::table("child"), = relx::ann::composite_fk<^^Parent::a, ^^Parent::c>("pa", "pc")]]
    Child {
  int pa;
  int pc;
};

int main() {
  (void)relx::schema::table_constraints_sql<Child>();
}
