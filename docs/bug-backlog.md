# Bug backlog

From a production-hardening audit (2026-08-05). Line numbers are anchors as of the audit; verify before fixing.

## Tier 1 — silent data corruption

### Result parsing
- [x] ~~`Cell::as<std::string>()` routes raw libpq cells through `column_traits::from_sql_string` SQL-literal de-quoting~~ Fixed 2026-08-05: `from_sql_string` now parses raw protocol text verbatim for string/enum/uuid traits (no de-quoting), and `Cell::as<std::string>` returns the raw value directly.
- [x] ~~SQL NULL stored as the string `"NULL"`~~ Fixed 2026-08-05: `Cell` carries an out-of-band `is_null_` flag (`Cell::null()` factory) populated from `PQgetisnull`; the internal pipe-text format (streaming sources, `parse`, `parse_lazy`) uses a COPY-style `\N` marker with backslash escaping of `\`, `|`, and line breaks, so real `"NULL"`/`\N` text round-trips. `column_traits<std::optional<T>>::from_sql_string` no longer sniffs `"NULL"`. Bonus: `PostgreSQLConnection::execute_typed(nullptr)` now binds a real SQL NULL via `bind_param::null()` instead of the text `"NULL"` (the old in-band sentinel masked this in tests).

### Prepared statements
- [x] ~~`PostgreSQLStatement::execute()` splices params into `EXECUTE name(...)` text with a hand-rolled escaper~~ Fixed 2026-08-05: rewritten on `PQexecPrepared`; `escape_string` deleted.

### Migrations generate invalid/destructive SQL
- [x] ~~`DROP CONSTRAINT` names are fabricated positionally but constraints are created unnamed~~ Fixed 2026-08-05: migration `ADD` now embeds the tracked name (`ADD CONSTRAINT <name> ...`) so the paired `DROP CONSTRAINT` targets a real name, and an explicit `.named()` overrides the generated one. Residual: constraints born inside `CREATE TABLE` still carry PG auto-names, which a later generated `DROP CONSTRAINT` won't match.
- [x] ~~`ALTER TABLE ... DROP PRIMARY KEY` is MySQL syntax~~ Fixed 2026-08-05: emits `DROP CONSTRAINT <name>`.
- [x] ~~Column modification becomes DROP + ADD — unconditional data loss~~ Fixed 2026-08-05: `ModifyColumnOperation` now emits in-place `ALTER COLUMN ... TYPE ... USING <col>::<type>`, `SET/DROP NOT NULL`, and `SET/DROP DEFAULT`; the differ uses it when `preserve_data` (default) and the change is expressible in place (`can_express`), falling back to drop+add otherwise.
- [x] ~~Zero identifier quoting in generated DDL~~ Fixed 2026-08-05: constexpr `relx::schema::quote_identifier` (`schema/identifier.hpp`) quotes reserved keywords, mixed case, and special characters (safe lowercase identifiers stay bare, so generated SQL is unchanged for normal names). Applied across column/table DDL, hoisted and annotation constraints, migration operations, and the query paths (qualified column refs, FROM/JOIN/INTO/UPDATE/DELETE table names, INSERT column lists, UPDATE SET).

## Tier 2 — hangs, corruption, resource bugs (connection layer)

- [x] ~~Pool validation runs `SELECT 1` while holding `pool_mutex_`~~ Fixed 2026-08-05: `get_raw_connection` pops the idle entry, releases the mutex, validates, and re-locks; a dead connection frees its slot and notifies a waiter.
- [x] ~~Streaming single-row mode never drains `PQgetResult` to NULL~~ Fixed 2026-08-05 for the sync source; verified and fixed the async source the same day (`drain_results()` on end-of-results and error paths in both `start_query` and `get_next_row`).
- [x] ~~Async `close()` double-closes libpq's fd~~ Fixed 2026-08-05: `close()` now does `socket_->release()` and lets `PQfinish` close the fd it owns.
- [x] ~~Sync streaming source move-assign on a reference member~~ Fixed 2026-08-05: `connection_` is now a pointer that rebinds on move.
- [x] ~~Pool `max_size` TOCTOU~~ Fixed 2026-08-05: the slot is reserved (`++total_connections_`) under the mutex before releasing it to connect, and released again on failure — concurrent creators cannot overshoot the cap.
- [x] ~~`initialize()` holds `pool_mutex_` across N blocking `PQconnectdb` calls; not idempotent~~ Fixed 2026-08-05: slots are reserved under the mutex, connections are established with the mutex released, and a repeat call tops up to `initial_size` instead of clobbering the count. On partial failure the successfully created connections are kept and unused reservations released.
- [x] ~~`size_t` pool counters underflow unguarded~~ Fixed 2026-08-05: all decrements go through guarded compare-exchange helpers that clamp at zero.
- [x] ~~`PooledConnection` defaulted move-assignment drops the connection without `return_connection`~~ Fixed 2026-08-05: move-assignment returns the held connection to the pool before taking the new one. The async pool header is gone (next item).
- [x] ~~`throw ConnectionError{...}` throws a non-`std::exception` aggregate~~ Fixed 2026-08-05: `prepare_statement` returns `ConnectionResult<std::unique_ptr<PostgreSQLStatement>>`; no `throw ConnectionError` remains.
- [x] ~~`connect()` builds error after `PQfinish` + null~~ Fixed 2026-08-05: message and status are captured before `PQfinish`.
- [x] ~~Result-status switch has no `default:`~~ Fixed 2026-08-05: the switch moved into `validate_exec_status` with a `default:` that errors on unknown statuses.
- [x] ~~`create_socket()` hardcodes `tcp::v4()`~~ Fixed 2026-08-05: the async wrapper now wraps libpq's fd in a protocol-agnostic `boost::asio::posix::stream_descriptor`, so IPv4, IPv6, and unix-socket conninfo all work.
- [x] ~~Async `Connection` move leaves `PreparedStatement::conn_` references pointing at the moved-from object~~ Fixed 2026-08-05: `PreparedStatement::conn_` is a rebinding pointer and `Connection`'s move constructor/assignment re-point every registered statement at the new object.
- [x] ~~Async `disconnect()` in destructor/`operator=` never awaited~~ Fixed 2026-08-05: destructor and move-assignment call the synchronous `async_conn_->close()` directly.
- [x] ~~`reset_connection_state_sync()` returns `true` while `PQisBusy`~~ Fixed 2026-08-05: it now returns `false` when results are still pending, input fails, or draining throws — `true` only when fully drained.
- [x] ~~Async `Result::ok()` rejects `PGRES_SINGLE_TUPLE`~~ Fixed 2026-08-05: `ok()` accepts it.
- [x] ~~`PostgreSQLAsyncConnection::begin_transaction` never sets `in_transaction_`~~ Resolved 2026-08-05: transaction state lives solely in the wrapper (`pgsql_async_wrapper::Connection::in_transaction_`, set by begin/commit/rollback and carried by moves); the stale shadow member on `PostgreSQLAsyncConnection` was deleted.
- [x] ~~`to_connection_string()` doesn't quote/escape values~~ Fixed 2026-08-05: every string value is single-quoted with `\`/`'` backslash-escaped per libpq's conninfo grammar.
- [x] ~~`postgresql_async_connection_pool.hpp` has no implementation anywhere~~ Resolved 2026-08-05 by deleting the header: nothing included it, nothing implemented it, and an async pool is a feature to design deliberately, not a stub to keep.
- [x] ~~`PostgreSQLStatement::execute_raw`/`process_result` are always-error stubs~~ Fixed 2026-08-05: deleted with the `PQexecPrepared` rewrite; execution goes through `PostgreSQLConnection::execute_prepared`.

## Tier 3 — silent wrong values (parsing/serialization)

- [x] ~~Structured-binding result path substitutes `ResultType{}` on failed conversion~~ Fixed 2026-08-05: the iterator throws `std::runtime_error` naming the column — iteration has no error channel, and fabricated zeros are worse than an exception.
- [x] ~~`ResultSet::as<Types...>(names)`: unknown column name silently binds by index~~ Fixed 2026-08-05: throws `std::invalid_argument`.
- [x] ~~`ColumnTypeConcept` satisfied by the undefined primary `column_traits` template~~ Fixed 2026-08-05: the primary template is now truly undefined (declaration only), so unsupported types fail the concept at compile time; numeric traits parse via a strict full-consumption `from_chars` helper (locale-independent, rejects partial parses).
- [x] ~~Unrecognized bool text maps to `false` with success~~ Fixed 2026-08-05: `convert_and_assign` returns an error and `column_traits<bool>::from_sql_string` throws on unrecognized text.
- [x] ~~`from_chars` partial parses accepted~~ Fixed 2026-08-05: `ptr == end` is required in `convert_and_assign` and the numeric traits.
- [x] ~~`std::isdigit(char)` on negative bytes is UB~~ Fixed 2026-08-05: all call sites take `unsigned char`; the `postgresql_statement.cpp` heuristic no longer exists (deleted with the `PQexecPrepared` rewrite).
- [x] ~~`column_traits<double/float>::to_sql_string` uses `std::to_string`~~ Fixed 2026-08-05: `std::format("{}")` (shortest round-trip, locale-independent).
- [x] ~~Streaming param serialization: `nullptr` → literal `"NULL"` text, `std::to_string(double)`~~ Fixed 2026-08-05: both streaming sources now carry `std::vector<bind_param>` end to end — `nullptr` binds a real SQL NULL, numerics travel with their type OID in binary, doubles format via `std::format`; query-object streaming passes `query.bind_params()` straight through (`streaming_params.hpp` deleted). The unused binary-params constructor was removed.
- [x] ~~`Value<std::chrono::year_month_day>` text form carries SQL quotes~~ Fixed 2026-08-05: `Value<year_month_day>` specialization (plus `val(ymd)`) binds the bare ISO date text.
- [x] ~~`year_month_day::from_sql_string` does no validation~~ Fixed 2026-08-05: strict `YYYY-MM-DD` shape check, full-consumption `from_chars` fields, and `ymd.ok()` validation; malformed or `infinity` input throws `std::invalid_argument`.
- [x] ~~Chrono parsing: `std::stoi`/`std::get_time`/`std::gmtime`~~ Fixed 2026-08-05: `chrono_traits.hpp` rewritten on `from_chars` + `std::chrono` calendar arithmetic and `std::format` — locale-independent, thread-safe, validating.
- [x] ~~`string_view` struct fields accepted by `convert_and_assign`~~ Fixed 2026-08-05: rejected with a `static_assert` pointing at `std::string`.
- [x] ~~`convert_placeholders_to_postgresql` doesn't understand dollar-quoting, `E''`, comments, JSONB `?`~~ Fixed 2026-08-05: the scanner now skips `--` and nested `/* */` comments, `'...'`, `E'...'` (backslash escapes), `"..."`, and `$tag$...$tag$` bodies; `??` escapes a literal `?` (the JSONB-operator convention).
- [x] ~~BYTEA hex decode failure returns the original hex string as the value~~ Fixed 2026-08-05: malformed hex throws `std::invalid_argument`; the execute path converts it to a `ConnectionError`, the sync streaming source records it in `last_error()`, and the async source ends the stream (all free the `PGresult` first).
- [x] ~~Join `bind_params()` exceptions caught, printed, and execution continues~~ Fixed 2026-08-05: rethrown with context — a query must not execute with missing parameters.
- [x] ~~`ResultSet::to_string()` writes newlines to `std::cout`~~ Fixed 2026-08-05: newlines go into the returned string.
- [x] ~~`LazyResult::operator[]` and iterator deref discard errors~~ Fixed 2026-08-05: both throw `std::out_of_range` with the underlying error; `at()` remains the non-throwing form.
- [x] ~~`select(t.id)` on a temporary table object compiles and dangles~~ Fixed 2026-08-05: `ColumnRef` stores the (stateless, `static_assert`-verified) column by value, and `select_all`'s function-local static is gone.
- [x] ~~Migration constraint identity is positional~~ Fixed 2026-08-05: generated constraint names derive from the constrained columns (`users_unique_email`, `t_fk_user_id`) or a content hash for CHECK/unknown shapes — reordering struct fields no longer produces phantom drop/add pairs.
- [x] ~~`ColumnMetadata::operator==` compares full `sql_definition` strings~~ Fixed 2026-08-05: comparison is now name + sql_type + nullable + whitespace-normalized definition, and an in-place-expressible difference produces `ALTER COLUMN` rather than destructive drop+recreate.

## Tier 4 — error reporting defects

- [x] ~~SQLSTATE never captured~~ Fixed 2026-08-05: `ConnectionError` now carries `sql_state`/`detail`/`hint`/`constraint_name` populated from `PQresultErrorField` on the sync execute path, with classification helpers (`is_duplicate_key_error`, `is_foreign_key_violation`, `is_check_constraint_violation`, `is_not_null_violation`, `is_serialization_failure`, `is_deadlock`); dead `PostgreSQLError`/`sql_state_map` deleted; `docs/error-handling.md` example rewritten. Residual: the async path (`PgError`) still reports flat messages.
- [x] ~~`sql_state_map` covers only class 23~~ Superseded: the map is gone; helpers compare `sql_state` directly and now include 40001/40P01. Other codes are readable straight off `error.sql_state`.
- [x] ~~`StreamingError::is_recoverable()` misclassifies~~ Resolved 2026-08-05 by deleting the never-referenced `streaming_errors.hpp` entirely.
- [x] ~~Unconditional `std::cerr`/`std::cout` writes from library code~~ Fixed 2026-08-05: the statement destructor/move-assign no longer print (no error channel there; a failed DEALLOCATE resolves at connection close), the NONFATAL-notice `cerr` is gone (notices belong to `PQsetNoticeReceiver`), `ResultSet::to_string` no longer touches `cout`, and the join-params `std::print` became a rethrow. `streaming_errors.hpp` was already deleted. Remaining console output is the migrations CLI tool, which is legitimately a CLI, plus a compile-time-disabled `ultra_verbose` debug block (see the logging-hooks hardening item).
- [x] ~~`format_error` has no `MigrationError` overload~~ Fixed 2026-08-05: overload added; `value_or_throw(generate_migration(...))` compiles and is covered by a test.
- [x] ~~Docs reference nonexistent `<relx/error.hpp>`; migration docs show pseudo-code~~ Fixed 2026-08-05: the `<relx/error.hpp>` reference was already gone after the docs rewrite; the migration samples now use the real API (`MigrationResult` unwrapping, `PostgreSQLConnection` transaction flow, `value_or_throw`).

## Not bugs — hardening features (separate effort)

- Query/statement timeout (`statement_timeout`, `PQcancel`), pool acquire covering connect+validate, TCP keepalive. Nothing in the async wrapper carries a deadline either — the `async_wait` loops at `connection/pgsql_async_wrapper.hpp:247,286,418,440` wait indefinitely. `connect_timeout` (`connection/connection.hpp:146`) covers establishment only.
- Reconnect/backoff, background health checks, `DISCARD ALL` on pool return
- Graceful pool shutdown: the destructor drops idle connections with no drain, no wait for outstanding borrows, and no way to stop handing out new ones (`src/postgres/connection/postgresql_connection_pool.cpp:8-14`)
- Paramless queries go through `PQexec` (`postgresql_connection.cpp:196`), which permits stacked statements. Needed for migration DDL; should not be the default execution path elsewhere.
- Transaction retry helper (e.g. `with_transaction_retry(conn, fn, policy)`) for
  serialization failures/deadlocks under Serializable. The classification half landed
  2026-08-05 (`ConnectionError::is_serialization_failure()` / `is_deadlock()`); only the
  retry loop with backoff remains.
- Logging/metrics hooks (replace the `cerr`/`ultra_verbose` TODOs)
- `sslmode` production default (`verify-full`) and documented TLS posture
- Savepoints/nested transactions; async transaction RAII
- Migration runner: history table, advisory lock, checksums, destructive-op flag
- Row-count/size guard on non-streaming results; stop copying column names per row
- LIKE metacharacter escape helper
- Live-PG integration tests for migrations and value round-trips (would have caught most of Tier 1)

## Test & CI gaps

CI is `.github/workflows/ci.yml`: format check, then Debug (coverage), Release, ASan+UBSan, TSan, and `-fanalyzer` jobs on GCC 16.

- [x] ~~No sanitizer run at all~~ Fixed 2026-08-05: `sanitize-asan-ubsan` runs the full suite (627 tests, leak detection on) and `sanitize-tsan` runs the pool/streaming tests, both verified green locally. UBSan's pointer-nullability checks (`null`, `nonnull-attribute`, `returns-nonnull-attribute`) are excluded — GCC 16's instrumentation for them breaks constant evaluation of the consteval SQL builders.
- [x] ~~Debug only~~ Fixed 2026-08-05: `linux-gcc-release` builds Release and runs the full suite (verified green locally).
- [x] ~~No static analysis~~ Fixed 2026-08-05: `gcc-analyzer` builds with `-fanalyzer` and summarizes findings in the job log (warnings don't fail the job; hard errors do). clang-tidy stays disabled until Clang ships P2996.
- [x] ~~No fuzzing on the parsers relx owns~~ Addressed 2026-08-05 within toolchain limits: libFuzzer needs Clang (can't parse P2996), so `test/fuzz/parser_fuzz_test.cpp` runs seeded randomized sweeps (5000 iterations each) over `convert_placeholders_to_postgresql`, `decode_binary_cell` (exposed via `decode_binary_cell_for_testing`), and both chrono `from_sql_string` parsers — with real teeth from the ASan/UBSan/TSan CI jobs they run under. Revisit coverage-guided fuzzing when Clang ships reflection.
- [x] ~~No failover/recovery test~~ Fixed 2026-08-05: `PoolRecoveryTest.SurvivesAllBackendsTerminated` stales every pooled connection at once via `pg_terminate_backend` and verifies the pool detects the corpses during validation and hands out working replacements (also runs under TSan).
- [x] ~~Single compiler, single platform~~ Accepted 2026-08-05 as a standing constraint, not a gap to close: GCC 16 is the only released toolchain implementing P2996 (see docs/development.md). Revisit when Clang ships reflection (~Clang 24).

---

# Round 2 — multi-agent audit, 2026-08-05 afternoon

Six parallel subsystem audits (query DSL, migrations, schema/traits, results, connection, coverage). Line numbers are anchors as of the audit; the tree concurrently carried uncommitted struct-writes work (`write_meta.hpp` and friends), which is out of scope here. Items marked *(verified)* were independently re-read or executed by the coordinating session; the pipe-format, migration-helper, and residual/`can_express` items were confirmed by running the extracted code in a harness.

## Tier 1 — memory corruption / UB

- [ ] *(verified)* `CoalesceExpr::bind_params()` folds `items.bind_params().begin()` / `.end()` from two **different** temporary vectors — `insert` computes a distance across unrelated allocations (measured garbage distances incl. 230 for 3 elements). Fires on 3+-arg `coalesce` with params in the `rest_` tuple; the commented-out `CoalesceMultipleValues` test says "seg faults sometimes". `include/relx/query/function.hpp:383-385`.
- [ ] *(verified)* Reversed `operator/` (value ⊘ ArithmeticExpr) instantiates `ArithmeticExpr<…, decltype(right_expr)>` where `decltype(right_expr)` is a const reference type — the stored member dangles once the RHS temporary dies. `include/relx/query/arithmetic.hpp:504-511` (+ duplicate ~:898 in `relx::schema`).
- [ ] `LazyRow` values returned by `LazyResultSet::at()`/`operator[]`/iterator use the non-owning constructor (string_views into `raw_data_`) — a row outliving or even a *move* of the result set dangles. Owning constructor exists (streaming uses it). `include/relx/results/lazy_result.hpp:258-302`.
- [ ] `StreamingResultSet` iterators hold `DataSource&` while the set stores the source by value with implicit moves — moving the set silently ends existing iterators. `include/relx/results/streaming_result.hpp:62,111`.
- [ ] Async wrapper `statements_` map mutated across `co_await`: iterator held over `deallocate()` await (`erase(it)` UB if another coroutine rehashes), and statements inserted **before** prepare completes are visible to peers. `include/relx/connection/pgsql_async_wrapper.hpp:626-651`.
- [ ] `PostgreSQLStatement` holds a raw `PostgreSQLConnection*`: destructor UAF if it outlives the connection; connection move orphans statements (no rebinding — the async wrapper got exactly this fix). `include/relx/connection/postgresql_statement.hpp:99`.
- [ ] Async `PreparedStatement` `shared_ptr`s advertise independent lifetime but dereference a raw `conn_` after `Connection` death; `close()` only clears the map. `pgsql_async_wrapper.hpp:147,343-360`.
- [ ] Async streaming source move-assign skips the `connection_` reference member — the sync twin was fixed for this today; result reads the wrong connection and strands the other in single-row mode. `src/postgres/connection/postgresql_async_streaming_source.cpp:39-67`.
- [ ] Async wrapper move-*assignment* moves the socket but `io_` is an unrebindable reference — cross-io_context assignment mismatches executor and fd (move ctor is correct). `pgsql_async_wrapper.hpp:329-341`.

## Tier 2 — silent data corruption

- [ ] *(verified by harness)* Pipe-text row format loses trailing empty cells: writers emit `a|b|` for a trailing `""`, all three splitters infer the last cell from a size check and drop it (25.1% of 200k random rows lost data; all-empty single-column row parses to **zero** cells; blank-line rows vanish in `parse`/`parse_lazy`). Live on both streaming paths. `result.hpp:920,931,959`, `lazy_result.hpp:234,367`.
- [ ] `parse_lazy` header parsing drops empty column names (`lazy_result.hpp:393`) while `parse` keeps them — name→index mapping shifts and `get<T>("name")` reads the **wrong column**. Neither reader unescapes header names.
- [ ] Async streaming source swallows every error — non-TUPLES_OK, `PQconsumeInput` failure, socket error, decode exception all `co_return nullopt`; no `last_error()` exists, so a stream killed at row 500k is indistinguishable from clean completion. `postgresql_async_streaming_source.cpp:113-176`.
- [ ] `reset_connection_state_sync()`'s fixed return value is discarded by both callers — the backlog fix is currently inert. `postgresql_async_streaming_source.hpp:177,198`.
- [ ] Duplicate result column names (`SELECT u.id, p.id …`) pass `verify_result_columns_consumed`, first match wins, second column's value silently dropped and its field default-initialized. `include/relx/connection/meta.hpp:98-158`.
- [ ] Tier-1 `"NULL"` sniff survives on `column<…, std::optional<T>>::from_sql_string` (public API), and `test/schema/optional_column_test.cpp:66-67` asserts the defect. `include/relx/schema/column.hpp:624-629`.
- [ ] Unsigned integrals bind as same-width **signed** OIDs binary-first: `uint32_t{4'000'000'000}` arrives as −294967296 with no error. `include/relx/bind_param.hpp:96-98,138-149`.
- [ ] `PostgreSQLStatement::execute_typed` serializes floats with `std::to_string` — 6 digits, locale-dependent (`1e-10` → `0.000000`). `postgresql_statement.hpp:66`. Same class of defect in `default_value<Value>::to_sql()` (`column.hpp:227`) and `make_array_bind_param` text form (`bind_param.hpp:224`).
- [ ] Enum CHECK is emitted inline, not hoisted, so adding an enumerator makes the differ rebuild the column via DROP+ADD — every row's value destroyed with `preserve_data` defaulted true. `column.hpp:446-449,608-611` → `constraint_operations.hpp:148-163,212` → `diff.cpp:130-135`.
- [ ] *(verified)* Migration phase order: column drops precede constraint drops, so dropping an FK/unique-bearing column emits `DROP CONSTRAINT` after PG already cascaded it (errors), and `rollback_sql`'s simple reversal breaks the other direction (ADD CONSTRAINT before ADD COLUMN). `src/migrations/diff.cpp:95-160`, `core.hpp:369-382`.
- [ ] Drop+add column rebuild silently discards constraints the differ sees as "unchanged" — no ADD CONSTRAINT regenerated. `diff.cpp:129-136`.
- [ ] Rename + type change with no `column_transformations` entry emits ADD + DROP with **no** data copy, silently; `test_migrations.cpp:935-941` asserts it as correct. `diff.cpp:60-75`.
- [ ] `ModifyColumnOperation` takes `USING col::type` unconditionally — truncating (`VARCHAR(255)`→`VARCHAR(10)`) and one-way (`INTEGER`→`TEXT`) casts silently accepted, and `rollback_sql` claims reversibility it doesn't have (`test_migrations.cpp:429-434` pins the false inverse).  `constraint_operations.hpp:180`.
- [ ] CLI writes multi-statement rollback SQL with only the **first** line commented — live rollback SQL inside the forward migration file. `src/migrations/command_line_tools.cpp:84`.
- [ ] `date_diff("month", …)` = `EXTRACT(MONTH FROM AGE(...))` — the interval *component* (14 months → 2), not total months. `include/relx/query/date.hpp:42-43`. Also: units dispatch on singulars while the doc instructs plurals, and unknown units emit nonexistent `DATE_DIFF(...)` (`date.hpp:32-49,424`).
- [ ] `LazyCell::get_raw_value()` unescapes `\N` to `"N"` for SQL NULL cells. `lazy_result.hpp:28-31`.
- [ ] Three boolean grammars disagree (`Cell::as`, `column_traits<bool>`, `convert_and_assign`): `get<bool>` rejects `"1"` where `get<optional<bool>>` accepts it — wrapping in optional silently loosens validation. `result.hpp:167-194`, `schema/core.hpp:103-114`, `meta.hpp:47-60`.
- [ ] `parse_value` optional branch converts a conversion **failure** into success-`nullopt` (currently shadowed, same in-band pattern the backlog eliminated). `result.hpp:305-311`.

## Tier 3 — wrong SQL, quoting, runtime errors

- [ ] *(verified)* `SchemaColumnAdapter::qualified_name()` skips `quote_identifier`, unlike its base and unlike `ColumnRef` — reserved-word/mixed-case columns are quoted in SELECT lists but bare in WHERE/JOIN/GROUP BY/ORDER BY/HAVING; also still holds a `const C&` member (the pattern the by-value refactor removed elsewhere). `include/relx/query/schema_adapter.hpp:38-46`.
- [ ] *(verified)* Consteval DDL builders append `table_name_of<T>()` unquoted (runtime builders quote): reserved-word table names break, and an unannotated `struct Users` creates `users` while every query targets `"Users"` — nothing matches. `include/relx/schema/annotated_table.hpp:672,722`.
- [ ] `ALTER COLUMN … TYPE` emitted before `SET DEFAULT`; PG rejects when the old default can't auto-cast. Correct order: DROP DEFAULT → TYPE USING → SET DEFAULT. `constraint_operations.hpp:179-196` (measured).
- [ ] `.named()` constraints: ADD embeds the name unquoted (PG folds), DROP quotes it — mixed-case names never match; name extraction stops at the first space. `constraint_operations.hpp:15-21,61-92`, `diff.hpp:185-191`, `annotated_table.hpp:404` (annotated_check name also unquoted).
- [ ] `default_expression` scans for constraint keywords inside string literals — `DEFAULT 'no CHECK needed'` truncates to an unterminated literal or misroutes to drop+add. `constraint_operations.hpp:126-145` (measured).
- [ ] `ADD COLUMN … NOT NULL` without default generated for populated tables (documented as the happy path); differ has the info to at least warn. `diff.cpp:99-105`.
- [ ] `generate_create_table_migration` omits `create_indexes_sql` and `create_enum_type_sql` — same schema reached via create vs. incremental diff differs; native-enum tables fail on fresh DBs; enum-value additions on native enums diff to nothing (no `ALTER TYPE … ADD VALUE`). `diff.hpp:348-353`.
- [ ] Native enum PG type names: lowercased unqualified identifier, unquoted, no namespace disambiguation — `enum class Order` is a syntax error; `ns_a::Status` vs `ns_b::Status` collide. `column.hpp:137-145`.
- [ ] Member-level annotations that aren't `ColumnModifier`s are silently dropped: `[[=relx::ann::check(…)]]` on a member compiles and the constraint doesn't exist; struct-level `ann::pk` likewise ignored. No `relx::check` alias exported. `annotated_table.hpp:151-156,473-515`, `schema.hpp:53-70`.
- [ ] `string_default` doesn't escape quotes (`O'Brien` → broken DDL) — disagrees with `column_traits<std::string>::to_sql_string`. `column.hpp:270`.
- [ ] `column_traits<year_month_day>::to_sql_string` doesn't zero-pad the year — pre-1000 dates fail its own parser and bind wrong text. `chrono_traits.hpp:162`.
- [ ] Timestamp parser accepts negative time/offset fields (`10:-5:45` → 09:55:45, offset `+-5:00` applied backwards) — upper-bound checks only. `chrono_traits.hpp:88,135`. Also: >4-digit years don't round-trip; PG's pre-1900 second-granularity offsets throw. `chrono_traits.hpp:31,80,124-126`.
- [ ] Empty `in()` list renders `IN ()` — PG syntax error; `AnyCondition`'s comment cites this as `in_any`'s reason to exist but `in()` has no guard. `condition.hpp:119-131,174-186`.
- [ ] Injection/escaping surface: `interval()`/`extract()`/`date_trunc()`/`date_diff()` unit strings and runtime `as(expr, alias)` spliced raw into SQL. `date.hpp:51,170,175,278`, `column_expression.hpp:86`. Mixed-case `TypedAlias` names also emit unquoted → PG folds → by-name DTO match silently falls back positional (`row_type.hpp:43-45`, `meta.hpp:144-157`).
- [ ] Pool: the two new off-mutex slot-release paths `notify_one()` without the mutex — lost wakeup, waiter sleeps the full timeout (deterministic at `max_size=1` + stale conn). `postgresql_connection_pool.cpp:106-111,128-134`. Also: idle-connection teardown (PQfinish, ROLLBACK round-trips) runs **under** `pool_mutex_` (`:215-263`); `initial_size <= max_size` unvalidated.
- [ ] Async paths call blocking `PQgetResult` drains (destructor/cleanup) — a synchronous network wait on the io_context thread; abandoning a large stream stalls the event loop. `postgresql_async_streaming_source.cpp:189-199,422-433`, `pgsql_async_wrapper.hpp:287-289`.
- [ ] Async `start_query` failure paths leave the query in flight / `finished_` unset — `has_more_rows()` stays true and each retry **re-sends the query**. `postgresql_async_streaming_source.cpp:255-265,315-323`.
- [ ] Sync prepared statements: no name registry — duplicate name is a server error; DEALLOCATE swallowed in aborted transactions leaves poisoned pooled connections (no DISCARD ALL). `postgresql_connection.cpp:460-482`.
- [ ] `TransactionGuard` discards `sql_state` — retry loops can't use the new `is_serialization_failure()`/`is_deadlock()` helpers through it. `transaction_guard.hpp:14-15`. (Also re-exported into the global namespace by `connection.hpp:149-150`.)
- [ ] Async `execute_raw` uses `PQsendQueryParams` unconditionally — multi-statement DDL scripts work sync, fail async. `pgsql_async_wrapper.hpp:515`.
- [ ] `columns_holder` enumerates `nonstatic_data_members_of(^^T)` directly while every other path uses base-walking `refl::member_array<T>()` — tables with base classes get DDL/table-object disagreement. `annotated_table.hpp:175-176`.
- [ ] Free `when(column,…)`/`else_(column)` overloads recurse into free functions that don't exist — cannot compile, dead API. `operators.hpp:653-670`.
- [ ] Differ constraint classification is substring matching (a CHECK containing "UNIQUE" misclassifies; PRIMARY KEY misclassification collides on `<table>_pk`); two FKs on one column collide by generated name — second one vanishes. `diff.hpp:162-193`. `MigrationOptions::constraint_mappings` and `RenameConstraintOperation` are dead code. `diff.hpp:29`, `core.hpp:265-297`.
- [ ] CLI: file-open/generation failures throw through the exit-code API (unhandled); no stream-state check after writing (truncated file, exit 0); unknown arguments silently ignored (`--ouput` typo → stdout, exit 0). `command_line_tools.cpp:51-86,140-180`.
- [ ] FK actions (`on_delete`/`on_update`) placed before `references` in the modifier list are overwritten in hoisted constraint defs. `column.hpp:339-354`.
- [ ] Misc small: `_fs` literal template form can never match a string literal (`fixed_string.hpp:65-69`); `SYSTEM_USER` (PG16) missing from reserved list and no 63-byte NAMEDATALEN diagnostic (`identifier.hpp`); `char` DTO field hits a non-compiling branch instead of a static_assert (`meta.hpp:43-46`); `get<unsigned>` fails at runtime, `get<optional<unsigned>>` at compile (`result.hpp:277-316`); `Cell::as<long double>` via locale-dependent `std::stold` (`result.hpp:298-302`); `Cell::null()` stores literal `"NULL"` in the raw buffer (`result.hpp:132-136`); `result.hpp` missing `<algorithm>`/`<cctype>` includes, unused `<iostream>`; `get_column_name<MemberPtr>()` reads nonexistent `ColumnType::column_name` (`result.hpp:99-104`); uuid trait round-trip fails (quoted output not accepted back) and throws `std::runtime_error` where every other trait throws `std::invalid_argument`; `convert_bytea_` has no setter — BYTEA streaming conversion is dead code (`postgresql_streaming_source.cpp:15`); `static_pg_sql` doesn't mirror the runtime placeholder grammar it claims to (`static_sql.hpp:42-84`); `Value<time_point>` binds text so the binary timestamptz encoder is unreached (`date.hpp:868-872`); `select()` with zero columns renders `SELECT  FROM`; `from(a,b).join(c)` binds the join to the last table only; INSERT has no columns/values arity check; `update().set()` on the same column appends, contradicting its doc.

## Test & CI gaps (round 2)

- [ ] **compile_fail harness false pass**: the try_compile invocation lacks PostgreSQL include dirs, so `select_missing_row_field.cpp` fails on `libpq-fe.h: No such file` and `WILL_FAIL` scores it a pass — the DTO coverage `static_assert` is never instantiated (confirmed in the gcc:16 container). No case asserts diagnostic text (`PASS_REGULAR_EXPRESSION`); the control file includes only `schema.hpp`. `test/CMakeLists.txt:126-128`.
- [ ] `postgresql_streaming_test.cpp:47` is the tree's only `GTEST_SKIP` and guards the only sync-drain regression test — connection failure turns CI green with streaming untested. Make it a hard ASSERT.
- [ ] `test/postgres_integration/postgresql_integration_test.cpp` is not registered in `test/CMakeLists.txt` — never compiled.
- [ ] `async_dto_mapping_test.cpp:76` discards the connect result — `SetUp` completes vacuously without a DB.
- [ ] Pipe-format round-trip property test (`split(join(escape(cells))) == cells` over adversarial cells incl. `""`, `\N`, `|`, trailing backslash) — fails today; then deduplicate the three splitters. No writer→reader test exists at all (writer in src/postgres/, readers in include/, agreement unpinned).
- [ ] Binary decoders never triggered: NUMERIC (OID 1700, ~40 lines of base-10000 math), UUID (2950), negative int2/int8 sign extension. One fixed-vector table on `decode_binary_cell_for_testing` covers all three; the fuzz target currently discards its result (`parser_fuzz_test.cpp:68`).
- [ ] Backlog fixes a revert would not break: streaming pipe-escaping emit side; literal `'NULL'` text read-back; eager `parse` `\N` path; streaming NULL/double params; prepared-statement quote/backslash round-trip; `DROP CONSTRAINT <pk>`; identifier quoting through the query builder (no reserved-word table exists in test/query/ — all nine quote calls removable with suite green); `preserve_data=false` and `can_express` branches (0 grep hits); whitespace-insensitive `ColumnMetadata::operator==`; join bind_params rethrow; 4 of 6 SQLSTATE helpers (hardcoded `false` passes); pool TOCTOU/idempotent-initialize/underflow guards (one 8-thread test against max_size=10 never contends; `total_connections_` unobservable); `PooledConnection` move-assign; `rebind_statements` (test moves only statement-less connections).
- [ ] `TransactionGuard`: zero tests, pure logic, fake-connection testable — double commit, rollback-on-throw, swallowed rollback failure, move semantics. Highest-value server-free gap.
- [ ] `test/types/type_safety_test.cpp` asserts nothing: 11 `EXPECT_TRUE(true)`, all six invalid comparisons commented out, no `static_assert(!requires{…})` anywhere in the repo. The type-safety claim is unenforced; `TypeCompatible` (18 assert sites) has zero negative-compile coverage.
- [ ] Missing compile_fail cases by value: dangling `string_view` DTO field; `TypeCompatible` mismatch; stateful column rejected; unaliased aggregate in row; unsupported column type; order_by/sum on bool; case-arm mismatch; arithmetic on string.
- [ ] **Missing check, not missing test**: selecting a column from a table absent from FROM has no compile-time enforcement (provenance is available via `parent_table`) — the library's core premise, unimplemented.
- [ ] Weak assertions to fix: seven byte-identical CLI tests; nine fractional-second cases compared via `to_time_t` (truncates the thing under test); `TestErrorHandling` that tests nothing; fuzz oracles satisfiable by the identity function; four commented-out TEST blocks each hiding a real gap (3-arg coalesce "seg faults sometimes", CASE-in-UPDATE-SET, many-joins, arithmetic).
- [ ] No migration test mixes column + constraint changes (hides the phase-ordering bug); no rollback round-trip check; no enum-column diff; `ModifyColumnOperation` never exercised with two attributes changing at once; migration DDL never executed against live PG.
- [ ] Untested public API: `returning_all()` (all three writers), `UpdateQuery::where_in`, `autoincrement`/`identity<Start,Inc,…>` options (`grep GENERATED test/` → 0), `drop_table::restrict()`, `AliasedColumn` comparison operators, `query/literals.hpp` (3 dead UDLs), `reflect.hpp` (no dedicated test file), `write_migration_to_file`, `uuid_traits.hpp` (no test file).
- [ ] Fuzz additions: pipe-text round trip, bytea hex differential (two divergent copies = free oracle), conninfo quoting, `quote_identifier` (empty identifier yields `""` — PG rejects), chrono fuzz needs template mutation (currently 1 of 5000 inputs passes the shape gate).
