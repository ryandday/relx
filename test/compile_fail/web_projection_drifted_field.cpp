// Must fail to compile: the DTO names a field that no longer exists on the table,
// and enforce_projection fires a static_assert naming it.
#include <optional>
#include <string>

#include <relx/schema.hpp>
#include <relx/web/projection.hpp>

struct [[=relx::table("cf_events")]] Event {
  [[=relx::ann::pk]] int id;
  std::string title;
};

struct [[=relx::web::projects<Event>]] StaleDto {
  std::string renamed_title;  // drifted: no such column
};

int main() {
  relx::web::enforce_projection<StaleDto>();
}
