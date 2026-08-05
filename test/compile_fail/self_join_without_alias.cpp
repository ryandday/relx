// Must NOT compile: without table aliases a self-join emits the same table name
// twice, which PostgreSQL rejects
#include <string>

#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("users")]] Users {
  [[= relx::ann::pk]] int id;
  std::string username;
};

int main() {
  constexpr auto u1 = relx::t<Users>;
  constexpr auto u2 = relx::t<Users>;
  auto query = relx::query::select(u1.id).from(u1).join(u2, relx::query::on(u1.id != u2.id));
  (void)query;
}
