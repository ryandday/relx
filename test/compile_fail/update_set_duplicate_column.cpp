// Must NOT compile: two set() calls on the same column would append a duplicate
// assignment instead of replacing the first
// expect-error: column already has a SET clause
#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("tt")]] TT {
  int id;
  std::string name;
};
inline constexpr auto t = relx::t<TT>;

int main() {
  auto q = relx::query::update(t).set(t.name, "a").set(t.name, "b");
  (void)q;
}
