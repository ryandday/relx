// Must NOT compile: upsert() when the inserted columns do not cover the primary key
// (here the pk is an identity column that values_from skips)
// expect-error: primary-key column to be part of the inserted columns
#include <string>

#include <relx/query.hpp>
#include <relx/schema.hpp>

struct[[= relx::table("t")]] T {
  [[= relx::ann::pk, = relx::ann::identity]] int id;
  std::string name;
};

int main() {
  constexpr auto t = relx::t<T>;
  (void)relx::insert_into(t).values_from(T{.id = 0, .name = "x"}).upsert();
}
