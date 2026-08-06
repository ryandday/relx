// Must NOT compile: set_from patch field names a column the table does not have
#include <optional>
#include <string>

#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("t")]] T {
  [[= relx::ann::pk]] int id;
  std::string name;
};

struct BadPatch {
  std::optional<std::string> nickname;
};

int main() {
  constexpr auto t = relx::t<T>;
  (void)relx::update(t).set_from(BadPatch{.nickname = "x"});
}
