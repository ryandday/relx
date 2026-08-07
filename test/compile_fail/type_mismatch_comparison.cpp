// Must NOT compile: comparing an int column with a string value
// expect-error: Column type and value type are not compatible
#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("tt")]] TT {
  int id;
};
inline constexpr auto t = relx::t<TT>;

int main() {
  auto q = relx::query::select(t.id).from(t).where(t.id == "not_a_number");
  (void)q;
}
