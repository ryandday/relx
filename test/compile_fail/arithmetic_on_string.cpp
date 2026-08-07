// Must NOT compile: arithmetic on a string column
// expect-error: can only be performed on numeric columns
#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("tt")]] TT {
  std::string name;
};
inline constexpr auto t = relx::t<TT>;

int main() {
  auto e = t.name + t.name;
  (void)e;
}
