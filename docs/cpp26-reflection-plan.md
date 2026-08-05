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

## Phase 0 — Toolchain (done)

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

## Phase 1 — Replace Boost.PFR with reflection (done)

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

## Phase 4 — select_all expansion, table-level annotations, typed binds (done)

- `select_all(table)` expands to an explicit column list via reflection (classic tables
  and `t<Users>` both; constraint members are filtered out). No raw `*`: the statement
  is stable under column addition/reordering, and the query synthesizes a row type like
  any explicit select, so `fetch_all(select_all(users))` works.
- Struct-level annotations for constraints that span columns:
  `[[=relx::ann::composite_pk("a", "b")]]`, `composite_unique(...)`,
  `composite_fk<^^Other::x, ^^Other::y>("a", "b")`, `index_on(...)` (`.unique()` to make
  it unique), `check("expr")` (`.named("n")` to name it). Column names are strings — a
  struct-level annotation cannot reference `^^Self::member` while the struct is still
  incomplete — but every name is consteval-validated against the members when DDL is
  built (the static_assert message lists unknown names). Constraints land in
  `CREATE TABLE` (runtime and consteval builders); indexes come from
  `relx::create_indexes_sql<T>()` as one static CREATE INDEX statement per annotation.
- Typed bind parameters: the query layer's param currency is now `relx::bind_param` —
  the text form plus, for bool/int/float values, the SQL kind and exact big-endian wire
  bytes captured at `Value` construction (no float text round-trip). Both connections
  send tagged params with their type OID in binary format (`PQexecParams` /
  `PQsendQueryParams`); untagged params (strings, enums, chrono) remain untyped text
  with server-side inference, exactly as before. `bind_param` converts from and compares
  with strings, so string-typed call sites and tests work unchanged. Streaming sources
  and prepared statements still speak text (they take plain string parameters).

## Phase 5 — binary results, typed NULLs, constraint-aware migrations, native enums (done)

- **Binary result parsing**: typed select queries whose select list maps entirely to
  decodable types (bool, integers, floats, strings, enums, NUMERIC,
  `system_clock::time_point`, `year_month_day`) request `resultFormat=1`;
  `process_postgresql_result_binary` decodes cells OID-driven back into the canonical
  text forms the rest of the stack expects (float formatting at wire precision,
  base-10000 NUMERIC decode, timestamps as UTC). Unknown OIDs produce a clear error
  naming the column; anything not gated (raw SQL, aggregates without `value_type`)
  stays on the text protocol.
- **Typed NULL binds**: disengaged `optional` values render `?` and bind a typed NULL
  parameter (`bind_param::null(kind)`, `nullptr` to libpq) instead of splicing the
  literal `NULL` — SQL text no longer depends on parameter values, a prerequisite for
  prepared-statement reuse.
- **Chrono binds**: `time_point` → TIMESTAMPTZ and `year_month_day` → DATE binary wire
  encodings (microseconds/days since 2000-01-01).
- **Migrations see table-level annotations**: `extract_table_metadata` walks struct
  annotations of annotated tables, so composite keys/uniques/FKs, checks, and
  `index_on` indexes participate in schema diffs (ADD/DROP CONSTRAINT, CREATE/DROP
  INDEX).
- **Native Postgres enums** (opt-in): `[[=relx::ann::native_enum]]` /
  `column<..., E, native_enum>` stores the enum as a real `CREATE TYPE ... AS ENUM`
  type (name = lowercased enum identifier, DDL via `relx::create_enum_type_sql<E>()`)
  instead of TEXT + CHECK.
- **composite_fk validation**: referenced columns must all belong to one table and form
  its composite_pk / composite_unique / a pk- or unique-annotated column — a wrong FK
  is now a compile error instead of a runtime DDL failure.
- **Compile-fail tests**: `test/compile_fail/` + CTest (`WILL_FAIL`) covers the
  consteval validation diagnostics, with a must-compile control against flag rot.

## Phase 6 — consteval SQL for whole typed queries (done)

- The entire query-building chain (SELECT/INSERT/UPDATE/DELETE builders, conditions,
  operators, values, column refs) is constexpr: every `to_sql()` was rewritten from
  `std::stringstream` to string concatenation, and constructors/builder methods are
  marked constexpr. `SqlExpression`'s virtuals are constexpr (C++20 allows constexpr
  virtual dispatch), though the static path never actually dispatches - all calls go
  through concrete types.
- `relx::static_sql(query)` renders the statement at compile time into static storage
  (`define_static_string`); `relx::static_pg_sql(query)` additionally converts `?` to
  `$1, $2, ...` placeholders at compile time (same quote-aware algorithm as the runtime
  converter in sql_utils).
- Works because bind parameters carry values out-of-band - a disengaged optional binds
  a NULL parameter rather than changing the SQL text (Phase 5), so the statement is
  value-independent. Requirements: referenced table objects must be constant-expression
  usable (`relx::t<T>` or constexpr classic instances); aliased columns (shared_ptr)
  and dynamic IN-lists stay runtime-only, unchanged.
- UUID values bind as binary OID 2950 (16 bytes verbatim) and decode from binary
  results to canonical 8-4-4-4-12 text.

## Roadmap — Phases 7+ (agreed 2026-08-04)

Ordered by ergonomic payoff per effort; each phase is a dedicated pass.

- **Phase 7 — FK-derived joins**: `.join(posts)` synthesizes the ON clause from the
  `fk<^^Users::id>` / `composite_fk` annotation between the two tables. Two candidate
  FKs between the pair → static_assert directing to explicit `on()`.
- **Phase 8 — struct-based writes**: `insert(users).values_from(obj)` expands fields via
  reflection, skipping serial pk / defaulted columns; `.upsert()` derives the ON CONFLICT
  target from the pk annotation and SET list from non-pk fields; patch update from a
  struct of optionals (engaged fields only — inherently runtime SQL).
- **Phase 9 — nested row synthesis for joins**: `fetch_all` on a multi-table select
  groups columns by originating table into nested structs
  (`struct { Users user; Posts post; }`); left-joined side becomes `std::optional`.
- **Phase 10 — schema-drift verification**: `relx::verify_schema<Users, Posts>(conn)`
  diffs reflected metadata (reusing `migrations/diff.hpp` extraction) against
  `information_schema` and returns a structured error — boot-time drift check, no
  migration emitted.
- **Phase 11 — reflection utility types**: promote pick/omit/partial (prototyped in
  `crud-server/proto_utility_types.cpp` on the relx-web branch) into `relx::refl`;
  `where_equals(table, partialObj)` filter-by-example on top of `partial<T>`.
- **Phase 12 — struct-typed JSONB columns**: `column<..., Metadata, jsonb>` with
  reflection-driven encode/decode; includes binary bind/result support for JSONB.
- **Phase 13 — prepared-statement caching**: keyed on query type (`PQprepare` once,
  `PQexecPrepared` with binary params after); `static_pg_sql` supplies the statement
  text with zero runtime cost.
- **Phase 14 — small items**: `relx::debug::dump(row)` pretty-printer via
  `for_each_named_field`; consteval `$n` placeholder-count check for raw SQL literals.
- **Phase 15 — table aliases**: `relx::t<Users, "u">` puts the alias in the table_ref
  type, so two aliases of one table are distinct types, column refs emit `u.col`, and
  consteval SQL / static-shape memoization keep working. Unblocks self-joins (today a
  compile error — `tuple_contains_table_v` guards in select.hpp relax to same-table
  *and* same-alias), and is the prerequisite for subqueries in FROM, `UPDATE ... FROM`,
  and LATERAL. Phase 7's FK-derived joins need it for self-referential FKs
  (`manager_id → Users`).
