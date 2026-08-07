// Must NOT compile: a char struct field is ambiguous ('5' vs 5)
// expect-error: char-like struct fields are rejected
#include <relx/connection.hpp>
#include <relx/query.hpp>
#include <relx/schema.hpp>

int main() {
  char target = 0;
  auto r = relx::connection::convert_and_assign(target, std::string("x"));
  (void)r;
}
