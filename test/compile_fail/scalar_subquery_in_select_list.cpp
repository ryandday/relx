// Must NOT compile: a SELECT used as a select-list expression would be emitted
// without parentheses (SELECT SELECT ...)
// expect-error: scalar subqueries in a select list are not supported
#include <string>

#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("users")]] Users {
  [[= relx::ann::pk]] int id;
  std::string username;
};

struct[[= relx::table("posts")]] Posts {
  [[= relx::ann::pk]] int id;
  int user_id;
};

int main() {
  constexpr auto users = relx::t<Users>;
  constexpr auto posts = relx::t<Posts>;
  auto sub = relx::query::select(posts.id).from(posts);
  auto query = relx::query::select(users.id, sub).from(users);
  (void)query;
}
