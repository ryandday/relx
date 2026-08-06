// Must NOT compile: whole-table selects cannot synthesize rows for RIGHT joins
// (they can NULL out the FROM side, which nested synthesis does not model)
#include <string>

#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("a")]] A {
  [[= relx::ann::pk]] int id;
};

struct[[= relx::table("b")]] B {
  [[= relx::ann::pk]] int id;
  [[= relx::ann::fk<^^A::id>]] int a_id;
};

int main() {
  constexpr auto a = relx::t<A>;
  constexpr auto b = relx::t<B>;
  auto q = relx::select(a, b).from(a).right_join(b, relx::on(a.id == b.a_id));
  using Row = relx::row_type_for<decltype(q)>;
  (void)sizeof(Row);
}
