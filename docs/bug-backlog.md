# Bug backlog

From a production-hardening audit (2026-08-05). Line numbers are anchors as of the audit; verify before fixing.

## Tier 1 — silent data corruption

### Result parsing
- [ ] `Cell::as<std::string>()` routes raw libpq cells through `column_traits::from_sql_string` SQL-literal de-quoting — strips surrounding `'` and un-doubles `''` from real data (`results/result.hpp:155`, `schema/core.hpp:76-89`). Same for enums (`core.hpp:169`) and uuid (`schema/uuid_traits.hpp:27`). Note: `map_row_to_struct` path is correct; the two read paths disagree.
- [ ] SQL NULL stored as the string `"NULL"`; `Cell::is_null()` is `value_ == "NULL"` — real text `NULL` reads as SQL NULL (`src/postgres/connection/sql_utils.cpp:143,370`, `results/result.hpp:74`, `results/lazy_result.hpp:26`, `schema/core.hpp:198`). Needs an out-of-band null flag on `Cell`/`LazyCell`.

### Prepared statements
- [x] ~~`PostgreSQLStatement::execute()` splices params into `EXECUTE name(...)` text with a hand-rolled escaper~~ Fixed 2026-08-05: rewritten on `PQexecPrepared`; `escape_string` deleted.

### Migrations generate invalid/destructive SQL
- [x] ~~`DROP CONSTRAINT` names are fabricated positionally but constraints are created unnamed~~ Fixed 2026-08-05: migration `ADD` now embeds the tracked name (`ADD CONSTRAINT <name> ...`) so the paired `DROP CONSTRAINT` targets a real name, and an explicit `.named()` overrides the generated one. Residual: constraints born inside `CREATE TABLE` still carry PG auto-names, which a later generated `DROP CONSTRAINT` won't match.
- [x] ~~`ALTER TABLE ... DROP PRIMARY KEY` is MySQL syntax~~ Fixed 2026-08-05: emits `DROP CONSTRAINT <name>`.
- [ ] Column modification becomes DROP + ADD — unconditional data loss, no warning (`src/migrations/diff.cpp:118-130`). `ModifyColumnOperation` exists but is dead code; `MigrationOptions::preserve_data` is never read. Wire up `ALTER COLUMN ... TYPE ... USING`.
- [ ] Zero identifier quoting in generated DDL — table `order`, column `default`, or mixed case breaks (`migrations/core.hpp:165,242`, `schema/table.hpp:164,215`). Add a `quote_identifier` helper. Same gap on the query-building path: `ColumnRef::to_sql` emits bare `table.column` (`query/column_expression.hpp:43`) and FROM/JOIN emit bare table names (`query/select.hpp:117,144`), so the reserved-word break is silent there too. Not injection (identifiers are compile-time constants), but nothing catches it — quote unconditionally, or `static_assert` against the reserved-word list.

## Tier 2 — hangs, corruption, resource bugs (connection layer)

- [ ] Pool validation runs `SELECT 1` while holding `pool_mutex_` — serializes all checkouts; a stale socket blocks the mutex for the TCP retransmit window → app-wide deadlock (`src/postgres/connection/postgresql_connection_pool.cpp:55,92`).
- [x] ~~Streaming single-row mode never drains `PQgetResult` to NULL~~ Fixed 2026-08-05 for the sync source (`drain_results()` on every terminating path); VERIFY the async source's paths separately.
- [ ] Async `close()` double-closes libpq's fd: asio `socket_->close()` then `PQfinish` — can kill an unrelated fd assigned in between. `resync_socket()` does `release()` correctly; `close()` was missed (`connection/pgsql_async_wrapper.hpp:325-339`).
- [x] ~~Sync streaming source move-assign on a reference member~~ Fixed 2026-08-05: `connection_` is now a pointer that rebinds on move.
- [ ] Pool `max_size` TOCTOU: reads count, unlocks, creates — N threads overshoot the cap (`postgresql_connection_pool.cpp:61-75`).
- [ ] `initialize()` holds `pool_mutex_` across N blocking `PQconnectdb` calls, so a slow or unreachable DB blocks every other pool operation for `initial_size × connect_timeout`. Also not idempotent: `total_connections_ = config_.initial_size` clobbers rather than accumulates, so a second call leaks the first batch out of the count (`src/postgres/connection/postgresql_connection_pool.cpp:16-35`).
- [ ] `size_t` pool counters underflow unguarded (`postgresql_connection_pool.cpp:99,134,224`).
- [ ] `PooledConnection` defaulted move-assignment drops the connection without `return_connection` — permanent slot leak; same in async pool header (`connection/postgresql_connection_pool.hpp:174`, `postgresql_async_connection_pool.hpp:189`).
- [x] ~~`throw ConnectionError{...}` throws a non-`std::exception` aggregate~~ Fixed 2026-08-05: `prepare_statement` returns `ConnectionResult<std::unique_ptr<PostgreSQLStatement>>`; no `throw ConnectionError` remains.
- [ ] `connect()` builds error after `PQfinish` + null — always reports `CONNECTION_BAD`, real status lost (`postgresql_connection.cpp:104-110`).
- [x] ~~Result-status switch has no `default:`~~ Fixed 2026-08-05: the switch moved into `validate_exec_status` with a `default:` that errors on unknown statuses.
- [ ] `create_socket()` hardcodes `tcp::v4()` — breaks IPv6 and unix-socket conninfo (`pgsql_async_wrapper.hpp:197-211`).
- [ ] Async `Connection` move leaves `PreparedStatement::conn_` references pointing at the moved-from object (`pgsql_async_wrapper.hpp:144,166-174,305-323`). The sync half (`PostgreSQLStatement::operator=` not rebinding `connection_`) was fixed 2026-08-05 by making it a rebinding pointer.
- [ ] Async `disconnect()` in destructor/`operator=` is `[[maybe_unused]] auto _ = disconnect();` — lazy awaitable never awaited, body never runs (`src/postgres/connection/postgresql_async_connection.cpp:23,38`).
- [ ] `reset_connection_state_sync()` returns `true` while `PQisBusy` with results still pending (`postgresql_async_connection.cpp:270-315`).
- [ ] Async `Result::ok()` rejects `PGRES_SINGLE_TUPLE` (`pgsql_async_wrapper.hpp:96-102`).
- [ ] `PostgreSQLAsyncConnection::begin_transaction` never sets `in_transaction_`; move ctor doesn't carry it (`postgresql_async_connection.cpp:128-166`).
- [ ] `to_connection_string()` doesn't quote/escape values — password with space or quote corrupts conninfo (`connection/connection.hpp:57-97`).
- [ ] `postgresql_async_connection_pool.hpp` has no implementation anywhere — link errors if used. Implement or delete.
- [x] ~~`PostgreSQLStatement::execute_raw`/`process_result` are always-error stubs~~ Fixed 2026-08-05: deleted with the `PQexecPrepared` rewrite; execution goes through `PostgreSQLConnection::execute_prepared`.

## Tier 3 — silent wrong values (parsing/serialization)

- [ ] Structured-binding result path substitutes `ResultType{}` on failed conversion — loops iterate over zeros with no error (`results/result.hpp:586-591`).
- [ ] `ResultSet::as<Types...>(names)`: unknown column name silently binds by index instead of erroring — typo or PG case-folding reads the wrong column (`results/result.hpp:719-722`).
- [ ] `ColumnTypeConcept` satisfied by the undefined primary `column_traits` template — unsupported types fail at link time; `parse_value` validators (`result.hpp:189-325`) are dead code; `as<double>` hits locale-dependent `stod` (`schema/core.hpp:20-33,219-224`).
- [ ] Unrecognized bool text maps to `false` with success — in both `convert_and_assign` (`connection/meta.hpp:43-47`) and `column_traits<bool>::from_sql_string` (`schema/core.hpp:99-102`).
- [ ] `from_chars` partial parses accepted: `"12abc"` → 12 — `ptr` discarded (`connection/meta.hpp:57-60`).
- [ ] `std::isdigit(char)` on negative bytes is UB (`results/result.hpp:266,280,300`; also `postgresql_statement.cpp` numeric heuristic).
- [ ] `column_traits<double/float>::to_sql_string` uses `std::to_string` — 6-digit truncation + locale-dependent; live on text-param paths (streaming, prepared, debug log), masked on binary paths (`schema/core.hpp:53,110`). Use `std::format` like the read side does.
- [ ] Streaming param serialization: `nullptr` → literal `"NULL"` text, `std::to_string(double)` (`connection/postgresql_streaming_source.hpp`, async source). Migrate to `bind_param`. The dead bool branches (bool checked after `is_arithmetic_v`) were fixed 2026-08-05 in both streaming sources and `PostgreSQLStatement::execute_typed` (which also now maps `nullptr` to a real SQL NULL via `std::nullopt`).
- [ ] `Value<std::chrono::year_month_day>` text form carries SQL quotes from `chrono_traits.hpp:236-242` — `time_point` was fixed for exactly this (`query/date.hpp:857-871`), ymd missed. Add specialization.
- [ ] `year_month_day::from_sql_string` does no validation — malformed/`infinity` dates yield garbage silently (`schema/chrono_traits.hpp:244-255`).
- [ ] Chrono parsing: `std::stoi` on fraction outside the try block (`chrono_traits.hpp:73`), locale-sensitive `std::get_time` (`:166,174`), non-thread-safe `std::gmtime` (`:19-39`).
- [ ] `string_view` struct fields accepted by `convert_and_assign` — views into a destroyed `ResultSet` dangle after `execute_many` returns (`connection/meta.hpp:38-41`). Reject with `static_assert`.
- [ ] `convert_placeholders_to_postgresql` doesn't understand dollar-quoting, `E''` backslash escapes, `--`/`/* */` comments, or JSONB `?` operators (`src/postgres/connection/sql_utils.cpp:17-67`).
- [ ] BYTEA hex decode failure returns the original hex string as the value (`sql_utils.cpp:98-101`).
- [ ] Join `bind_params()` exceptions caught, printed, and execution continues with missing parameters (`query/select.hpp:217-225`).
- [ ] `ResultSet::to_string()` writes newlines to `std::cout` instead of the returned string (`results/result.hpp:792-799`).
- [ ] `LazyResult::operator[]` and iterator deref discard errors and return default-constructed rows (`results/lazy_result.hpp:266-273,287-293`).
- [ ] `select(t.id)` on a temporary table object compiles and dangles (`ColumnRef` stores `const Column&`, `query/column_expression.hpp:58`; `select_all` shares a mutable function-local static, `query/select.hpp:623-628`).
- [ ] Migration constraint identity is positional (`constraints.size()` at insertion) — reordering struct fields produces phantom drop/add pairs (`migrations/diff.hpp:95-112`). Fixed by content-derived names.
- [ ] `ColumnMetadata::operator==` compares full `sql_definition` strings — any cosmetic DDL change triggers destructive drop+recreate (`migrations/diff.hpp:52-54`).

## Tier 4 — error reporting defects

- [x] ~~SQLSTATE never captured~~ Fixed 2026-08-05: `ConnectionError` now carries `sql_state`/`detail`/`hint`/`constraint_name` populated from `PQresultErrorField` on the sync execute path, with classification helpers (`is_duplicate_key_error`, `is_foreign_key_violation`, `is_check_constraint_violation`, `is_not_null_violation`, `is_serialization_failure`, `is_deadlock`); dead `PostgreSQLError`/`sql_state_map` deleted; `docs/error-handling.md` example rewritten. Residual: the async path (`PgError`) still reports flat messages.
- [x] ~~`sql_state_map` covers only class 23~~ Superseded: the map is gone; helpers compare `sql_state` directly and now include 40001/40P01. Other codes are readable straight off `error.sql_state`.
- [x] ~~`StreamingError::is_recoverable()` misclassifies~~ Resolved 2026-08-05 by deleting the never-referenced `streaming_errors.hpp` entirely.
- [ ] Unconditional `std::cerr`/`std::cout` writes from library code (`postgresql_connection.cpp:262`, `postgresql_statement.cpp:24,42`, `streaming_errors.hpp:205`).
- [ ] `format_error` has no `MigrationError` overload — `value_or_throw(generate_migration(...))` doesn't compile (`utils/error_handling.hpp:29-43`).
- [ ] Docs reference nonexistent `<relx/error.hpp>` (`docs/error-handling.md:238`); migration docs show pseudo-code API that doesn't compile and drops `std::expected` results (`docs/migrations.md:578-609`).

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

CI is `.github/workflows/ci.yml`: format check, then one Debug build on GCC 16 with coverage.

- [ ] No sanitizer run at all. The pool is multithreaded and the async path mixes coroutines with raw `PGconn*` — ASan/UBSan/TSan is exactly where most of Tier 2 lives (the double-close at `pgsql_async_wrapper.hpp:325-339`, the reference-member move-assign, the counter underflows). Add ASan+UBSan on the unit suite and TSan on the pool tests.
- [ ] Debug only — no Release/RelWithDebInfo build, so no `NDEBUG` codegen path and no optimizer-visible UB is ever exercised.
- [ ] No static analysis. clang-tidy is disabled for a real reason (clang 20 cannot parse P2996; see the commented-out job), but nothing replaced it — GCC's `-fanalyzer` works today and would cover part of the gap.
- [ ] No fuzzing on the parsers relx owns, all of which consume input it does not control: `convert_placeholders_to_postgresql` (`src/postgres/connection/sql_utils.cpp:17-67`), `decode_binary_cell` (`sql_utils.cpp:191-346` — hand-rolled `numeric` wire decoding doing arithmetic on server-supplied `ndigits`/`weight`/`dscale`), and `chrono_traits::from_sql_string` (`schema/chrono_traits.hpp:40-140`).
- [ ] No failover/recovery test — nothing exercises a DB restart mid-pool, where every pooled connection goes stale at once. Would cover the validation-under-mutex deadlock and the reconnect/backoff gap together.
- [ ] Single compiler, single platform. Acceptable while GCC 16 is the only toolchain implementing P2996 — revisit when Clang ships reflection.
