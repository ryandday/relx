// Must NOT compile: a std::string_view struct field would dangle once the result
// set is destroyed
// expect-error: string_view struct fields are rejected
#include <relx/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

int main() {
  std::string_view target;
  auto r = relx::connection::convert_and_assign(target, std::string("x"));
  (void)r;
}
