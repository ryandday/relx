// Must NOT compile: upsert() on a table with no declared primary key
// expect-error: requires the table to declare a primary key
#include <string>

#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("t")]] T {
  std::string name;
};

int main() {
  constexpr auto t = relx::t<T>;
  (void)relx::insert_into(t).values_from(T{.name = "x"}).upsert();
}
