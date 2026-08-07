// Must NOT compile: the result struct does not cover the select list
// expect-error: has no field
#include <relx/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("users")]] Users {
  [[= relx::ann::pk]] int id;
  std::string username;
};

struct OnlyId {
  int id;
};

int main() {
  constexpr auto users = relx::t<Users>;
  auto query = relx::query::select(users.id, users.username).from(users);
  relx::connection::assert_struct_covers_select_list<OnlyId, decltype(query)>();
}
