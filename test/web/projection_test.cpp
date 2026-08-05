#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <relx/schema.hpp>
#include <relx/web/projection.hpp>

namespace {

// clang-format off: annotation syntax is not yet understood by clang-format

struct [[=relx::table("proj_events")]] Event {
  [[=relx::ann::pk, =relx::ann::identity]] int id;
  int owner_id;
  std::string title;
  std::optional<std::string> location;
  bool is_all_day;
};

// A create shape: subset of columns, matching types
struct [[=relx::web::projects<Event>]] CreateEvent {
  std::string title;
  std::optional<std::string> location;
};

// A patch shape: DTO may add optionality over non-optional columns
struct [[=relx::web::projects<Event>]] PatchEvent {
  std::optional<std::string> title;
  std::optional<std::string> location;
  std::optional<bool> is_all_day;
};

// No annotation: never checked, never fails
struct Unrelated {
  std::string whatever;
};

// clang-format on

TEST(ProjectionTest, ValidProjectionsHaveEmptyDiagnostics) {
  EXPECT_TRUE(relx::web::projection_diagnostic<CreateEvent>().empty());
  EXPECT_TRUE(relx::web::projection_diagnostic<PatchEvent>().empty());
  relx::web::enforce_projection<CreateEvent>();
  relx::web::enforce_projection<PatchEvent>();
}

TEST(ProjectionTest, UnannotatedDtoIsNotChecked) {
  EXPECT_TRUE(relx::web::projection_diagnostic<Unrelated>().empty());
  relx::web::enforce_projection<Unrelated>();
}

TEST(ProjectionTest, DiagnosticNamesDriftedFields) {
  // Checked via the diagnostic function (the compile-fail suite covers enforce_projection)
  struct Bad {
    std::string titel;  // typo
    int title;          // wrong type
  };
  struct[[= relx::web::projects<Event>]] Probe {};
  // Diagnose Bad against Event directly through the detail entry point
  constexpr std::string_view diag = std::define_static_string(
      relx::web::detail::projection_errors<Bad, Event>());
  EXPECT_NE(diag.find("titel"), std::string_view::npos);
  EXPECT_NE(diag.find("'title' type differs"), std::string_view::npos);
}

}  // namespace
