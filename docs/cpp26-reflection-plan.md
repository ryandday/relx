# C++26 Reflection (P2996) Adoption Plan

Goal: use compile-time reflection to remove API boilerplate and fix structural weaknesses in
result mapping. Research date: 2026-08-03.

## Compiler decision: GCC 16.1

GCC 16.1 (2026-04-30) is the only official compiler release with C++26 reflection.

| Feature | GCC 16.1 | Clang 22.1.8 (latest release) | Clang 23 (RC) |
|---|---|---|---|
| P2996R13 Reflection | ✅ `-freflection` | ❌ | ❌ (parses `^^int` only) |
| P1306 Expansion statements (`template for`) | ✅ no flag needed | ❌ | ⚠️ partial |
| P3394 Annotations `[[=x]]` | ✅ | ❌ | ❌ |
| P3491 `define_static_string/array/object` | ✅ | ❌ | ❌ |
| P3293 / P3096 / P3560 | ✅ | ❌ | ❌ |
| stdlib `<meta>` | ✅ libstdc++ (704 lines) | ❌ none in libc++ | ❌ |

Clang's first metafunction (`std::meta::is_type`) is an unmerged draft PR; full P2996 is
realistically Clang 24+ (~2027). Verified locally: all idioms this plan relies on compile and run
under the `gcc:16` Docker image (16.1.0).

Caveats to build around:

- `<meta>` is **silently empty** without `-freflection` — guard with
  `static_assert(__cpp_impl_reflection >= 202603L)`.
- Requires the default (new) libstdc++ ABI; `_GLIBCXX_USE_CXX11_ABI=0` disables `<meta>`.
- GCC marks C++26 experimental; expect churn in 16.x → 17.
- clang-format 20 doesn't know `^^` / `[: :]` / `template for` — wrap reflection-dense code in
  `// clang-format off` regions. clang-tidy 20 cannot parse it at all — CI job disabled until
  Clang 24+.
- This makes relx GCC-only until Clang catches up (previously it was Clang-only; there is no
  two-compiler option in 2026).

## Phase 0 — Toolchain (this PR)

- `CMakeLists.txt`: `cmake_minimum_required` → 3.25+ (needed for `cxx_std_26`),
  `CMAKE_CXX_STANDARD 26`, `target_compile_features(relx INTERFACE cxx_std_26)`,
  `target_compile_options(relx INTERFACE $<$<CXX_COMPILER_ID:GNU>:-freflection>)` so consumers
  inherit the flag.
- CI: replace the Clang 20 job with a `gcc:16` container job. Postgres runs as a GitHub Actions
  service; tests hardcode `localhost:5434`, so the job forwards it with
  `socat TCP-LISTEN:5434,fork TCP:postgres:5432`. clang-format job stays (format only);
  clang-tidy job disabled.
- Local dev on macOS: no Homebrew GCC 16 yet — build/test through the `gcc:16` Docker image
  (see `docker-dev/Dockerfile` for the image and usage commands).

## Phase 1 — Replace Boost.PFR with reflection (this PR)

New `include/relx/reflect.hpp`:

```cpp
template <typename T> consteval auto member_array();       // define_static_array of nsdms
template <typename T> consteval std::size_t field_count();
template <typename T, typename Fn> constexpr void for_each_field(T&& obj, Fn&& fn);
// fn(field_ref, name) — name is the member identifier, static storage
```

Callers converted (all four PFR sites):

1. `schema/table.hpp` — `collect_column_definitions` / `collect_constraint_definitions`.
2. `migrations/diff.hpp` — `extract_table_metadata` (+ drop unused PFR include in
   `migrations/core.hpp`).
3. `connection/meta.hpp` — rewritten as `map_row_to_struct<T>(row)`; used by
   `connection.hpp` `execute<T>` / `execute_many<T>`.
4. `connection/postgresql_async_connection.hpp` — same mapper, deleting the duplicated zip.

Behavioral fixes that fall out of the new mapper:

- **Name-matched mapping**: DTO field names are matched against result column names
  (what `docs/result-parsing.md` always claimed); falls back to position for unnamed/aliased
  columns. Shuffled-field DTOs stop silently receiving wrong columns.
- **`std::optional<T>` DTO fields work**: NULL → `nullopt` (previously a compile error, despite
  being documented).
- NULL into a non-optional field is now a mapping error (previously assigned the literal string
  `"NULL"` or threw from `std::stoll`).
- Column-count mismatch error preserved as before.

Boost.PFR dependency removed entirely. Boost.system/thread/Asio remain (async client).

## Phase 2 — Annotation-based DSL (done)

`include/relx/schema/annotated_table.hpp`. Tables are plain annotated aggregates:

```cpp
struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]]     int id;
                         std::string username;
  [[=relx::ann::unique]] std::string email;
  [[=relx::ann::fk<^^Categories::id>]] int category_id;
  [[=relx::default_value<true>{}]] bool active;
  std::optional<std::string> bio;   // nullable
};

inline constexpr auto users = relx::t<Users>;  // define next to the struct, once

auto q = relx::select(users.id, users.username)
             .from(users)
             .where(users.id == 42);
auto rows = conn.execute_many<Users>(q);  // the schema struct IS the DTO
```

How it works: each column member of `relx::t<Users>` is an instance of the *existing*
`schema::column<table_t<Users>, "id", int, primary_key>` — the specialization is built with
`std::meta::substitute` from the member's identifier, type, and annotations (any type with a
static `to_sql()` is a modifier, so all existing modifiers are valid annotations via
`Type{}` or the `relx::ann` shorthands). The table object itself is synthesized with
`define_aggregate`: one column member per field of the annotated struct, same names. The
entire operator/query/adapter machinery works unchanged, the classic explicit DSL remains
fully supported alongside, and `relx::c<^^Users::id>` exists as a standalone column
reference when no table object is in scope.

Not yet annotation-expressible (use the classic DSL or raw SQL): composite primary
keys/uniques/FKs, indexes, table-level check constraints.

Implementation gotcha: `type_of(annotation)` is cv-qualified (`const table`) — always
`remove_cv` before comparing against `^^table` or substituting modifier types.

## Phase 3 — Consteval SQL + synthesized rows (done)

- `row_type_for<Query>` synthesizes result-row types from the select list
  (`define_aggregate`); `conn.fetch_all(query)` / `fetch_one(query)` return them.
  Expressions alias via `relx::as<"name">(expr)` / `relx::as<"name", T>(expr)`.
- Compile-time column coverage: typed queries static_assert that every selected column
  has a same-named field in the result struct (message names the missing columns).
  Structs may have extra fields — a subset select leaves them default-initialized.
  Runtime, every result column must be consumed by some field.
- Enums are first-class columns: TEXT + generated `CHECK(col IN (...))` via
  `enumerators_of`, identifier binds/parsing, enum `DEFAULT`s.
- Consteval DDL for annotated tables: `relx::create_table_sql<T>().if_not_exists()
  .to_sql()` / `drop_table_sql<T>().if_exists().to_sql()` build the statement at
  compile time into static storage (the modifier `to_sql()` chain is constexpr;
  floating-point DEFAULTs remain runtime-only — no constexpr float formatting).

## Future ideas

- Expand `select_all(t<Users>)` to an explicit column list via reflection so it works
  with synthesized rows and is migration-stable (no raw `*`).
- Annotations for composite keys, indexes, and table-level checks.
- Typed bind parameters over libpq's binary protocol.
