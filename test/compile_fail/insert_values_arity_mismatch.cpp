// Must NOT compile: the values row does not match the declared column count
// expect-error: must match the number of inserted columns
#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("tt")]] TT {
  int id;
  std::string name;
};
inline constexpr auto t = relx::t<TT>;

int main() {
  auto q = relx::query::insert_into(t).columns(t.id, t.name).values(1);
  (void)q;
}
