// Must NOT compile: set_from patch fields must all be std::optional
// expect-error: must be a std::optional
#include <string>

#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("t")]] T {
  [[= relx::ann::pk]] int id;
  std::string name;
};

struct BadPatch {
  std::string name;  // not optional
};

int main() {
  constexpr auto t = relx::t<T>;
  (void)relx::update(t).set_from(BadPatch{.name = "x"});
}
