// Must NOT compile: select() with no columns would render "SELECT  FROM ..."
// expect-error: needs at least one column
#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("tt")]] TT {
  int id;
};
inline constexpr auto t = relx::t<TT>;

int main() {
  auto q = relx::query::select().from(t);
  (void)q;
}
