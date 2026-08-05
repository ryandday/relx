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
