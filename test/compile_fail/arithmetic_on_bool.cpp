// Must NOT compile: arithmetic on a bool column
// expect-error: can only be performed on numeric columns
#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("tt")]] TT {
  bool is_active;
};
inline constexpr auto t = relx::t<TT>;

int main() {
  auto e = t.is_active * 2;
  (void)e;
}
