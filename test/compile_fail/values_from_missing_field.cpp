// Must NOT compile: values_from object lacks a field for an insertable column
// expect-error: object does not match the table
#include <string>

#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("t")]] T {
  [[= relx::ann::pk]] int id;
  std::string name;
};

struct MissingName {
  int id;
};

int main() {
  constexpr auto t = relx::t<T>;
  (void)relx::insert_into(t).values_from(MissingName{.id = 1});
}
