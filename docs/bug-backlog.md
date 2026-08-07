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

Six parallel subsystem audits (query DSL, migrations, schema/traits, results, connection, coverage). Line numbers are anchors as of the audit. Fix pass completed 2026-08-06; every fix carries a regression test (unit, live-PG, or compile-fail).

## Tier 1 — memory corruption / UB

- [x] ~~`CoalesceExpr::bind_params()` folds iterators from two different temporary vectors~~ Fixed 2026-08-06: each `bind_params()` result is materialized once per item before insert; the "seg faults sometimes" `CoalesceMultipleValues` test is re-enabled plus a 5-arg params test.
- [x] ~~Reversed `operator/` (value ⊘ ArithmeticExpr) stores a const-reference member that dangles~~ Fixed 2026-08-06: stores `ArithmeticExpr<Left, Right>` by value. Bonus: the entire duplicate set of ArithmeticExpr-operand operators in `relx::schema` was deleted — an Expr operand always associates `relx::query` via ADL, and the duplicates made every `value ⊕ expr` call ambiguous (which is why the arithmetic chaining test was commented out).
- [x] ~~`LazyResultSet::at()`/`operator[]`/iterator hand out rows viewing `raw_data_`~~ Fixed 2026-08-06: `at()` builds an owning row (`LazyRowOutlivesResultSet` test destroys the set first).
- [x] ~~`StreamingResultSet` iterators hold `DataSource&` while the set moves~~ Fixed 2026-08-06: copy/move deleted on both the sync and async result sets (factories rely on prvalue elision).
- [x] ~~Async wrapper `statements_` map mutated across `co_await`~~ Fixed 2026-08-06: the old entry is erased before awaiting deallocate, and a new statement becomes visible only after `prepare()` succeeds.
- [x] ~~`PostgreSQLStatement` holds a raw `PostgreSQLConnection*`; destructor UAF; connection move orphans statements~~ Fixed 2026-08-06: statements register with the connection; moves rebind every registered statement, disconnect/destruction invalidates them (execute errors, destructor no-ops). `StatementOutlivesConnectionSafely` / `ConnectionMoveRebindsStatements` tests.
- [x] ~~Async `PreparedStatement` `shared_ptr`s dereference a raw `conn_` after `Connection` death~~ Fixed 2026-08-06: `close()` nulls every statement's `conn_`; prepare/execute/deallocate error on a null connection.
- [x] ~~Async streaming source move-assign skips the `connection_` reference member~~ Fixed 2026-08-06: `connection_` is a rebinding pointer, mirroring the sync twin.
- [x] ~~Async wrapper move-assignment can't rebind the `io_` reference~~ Fixed 2026-08-06: `io_` is a pointer, rebound on move-assign.

## Tier 2 — silent data corruption

- [x] ~~Pipe-text row format loses trailing empty cells; all-empty rows vanish~~ Fixed 2026-08-06: shared `text_format::split_cells` (N separators → N+1 cells) used by every reader; `\n` is a row terminator so blank lines are real single-empty-cell rows; `result::parse` additionally errors on row/header cell-count mismatch. `pipe_format_roundtrip_test.cpp` round-trips adversarial cells plus a 5000-iteration randomized sweep and pins writer↔reader agreement.
- [x] ~~`parse_lazy` header parsing drops empty column names and never unescapes~~ Fixed 2026-08-06: headers go through `split_cells` + `unescape` in both parsers; empty names keep their position.
- [x] ~~Async streaming source swallows every error~~ Fixed 2026-08-06: every failure path records `last_error()` (exposed on the source and the result set); `AsyncStreamingErrorHandling` asserts a failed query is distinguishable from an empty one.
- [x] ~~`reset_connection_state_sync()` return discarded by both callers~~ Fixed 2026-08-06: a failed sync reset sets `pending_reset_` on the connection and the next async execution drains the wire before sending — discarding the bool can no longer poison the connection.
- [x] ~~Duplicate result column names silently drop the second value~~ Fixed 2026-08-06: `verify_result_columns_consumed` rejects duplicate names (`DuplicateResultColumnIsError`).
- [x] ~~Tier-1 `"NULL"` sniff survives on `column<…, std::optional<T>>::from_sql_string`~~ Fixed 2026-08-06: sniff removed; the test that asserted the defect now asserts the out-of-band contract.
- [x] ~~Unsigned integrals bind as same-width signed OIDs~~ Fixed 2026-08-06: unsigned types bind the next wider signed wire type; `uint64_t` stays untyped text (no lossless encoding). Byte-exact tests.
- [x] ~~`std::to_string` float serialization (statement `execute_typed`, `default_value`, `make_array_bind_param`)~~ Fixed 2026-08-06: all three (plus `PostgreSQLConnection::execute_typed`) use `std::format("{}")`.
- [x] ~~Enum CHECK emitted inline → adding an enumerator rebuilds the column via DROP+ADD~~ Fixed 2026-08-06: the type-supplied CHECK is hoisted with the modifier constraints, so the change diffs as DROP+ADD CONSTRAINT (`TextEnumValueAdditionDiffsAsCheckConstraintChange`).
- [x] ~~Migration phase order: column drops precede constraint drops~~ Fixed 2026-08-06: `diff_tables` emits constraint drops → column operations → constraint adds; reversed rollback is therefore also valid (`ConstraintDropsPrecedeColumnDrops`).
- [x] ~~Drop+add column rebuild silently discards "unchanged" constraints~~ Fixed 2026-08-06: constraints mentioning a rebuilt column are explicitly dropped before and re-added after the rebuild.
- [x] ~~Rename + type change with no transformation emits ADD+DROP with no data copy~~ Fixed 2026-08-06: now a `VALIDATION_FAILED` error demanding a `column_transformations` entry; the test asserting the silent loss now asserts the error.
- [x] ~~`ModifyColumnOperation` unconditional `USING col::type`; rollback claims false reversibility~~ Fixed 2026-08-06: `column_transformations` entries flow into explicit forward/backward USING expressions (`ModifyColumnUsesProvidedUsingExpressions`); the default cast remains for compatible changes.
- [x] ~~CLI writes multi-statement rollback SQL with only the first line commented~~ Fixed 2026-08-06: every line is comment-prefixed (`write_commented`).
- [x] ~~`date_diff("month", …)` returns the interval month *component*~~ Fixed 2026-08-06: total months (`years*12 + months`); plural unit spellings accepted; unknown units throw `std::invalid_argument` at build time instead of emitting nonexistent `DATE_DIFF(...)`.
- [x] ~~`LazyCell::get_raw_value()` unescapes `\N` to `"N"`~~ Fixed 2026-08-06: NULL cells return empty text.
- [x] ~~Three boolean grammars disagree~~ Fixed 2026-08-06: one grammar (`schema::detail::parse_bool`, case-insensitive t/true/f/false) used by `Cell::as`, `column_traits<bool>`, and `convert_and_assign`; "1"/"0" only via the explicit `allow_numeric_bools` opt-in; optionals delegate to the inner type before trait dispatch so wrapping can't loosen parsing.
- [x] ~~`parse_value` optional branch converts conversion failure into success-`nullopt`~~ Fixed 2026-08-06: failures propagate as errors (`ConversionFailureIsAnErrorNotNull`).

## Tier 3 — wrong SQL, quoting, runtime errors

- [x] ~~`SchemaColumnAdapter::qualified_name()` skips `quote_identifier`; holds `const C&`~~ Fixed 2026-08-06: quotes table and column like `ColumnRef` (reserved-word columns now quoted in WHERE/JOIN/ORDER BY — `ReservedWordColumnQuotedEverywhere`), stores the stateless column by value.
- [x] ~~Consteval DDL builders append the table name unquoted~~ Fixed 2026-08-06: `create_table_sql`/`drop_table_sql` quote via `quote_identifier` (`ConstevalDdlQuotesTableName`).
- [x] ~~`ALTER COLUMN … TYPE` before `SET DEFAULT`~~ Fixed 2026-08-06: DROP DEFAULT → TYPE USING → SET DEFAULT (`ModifyColumnDropsDefaultBeforeTypeChange`).
- [x] ~~`.named()` constraints: ADD unquoted, DROP quoted; name extraction stops at first space~~ Fixed 2026-08-06: the `CONSTRAINT <name>` prefix (quoted names included) is parsed off into the metadata name and both ADD and DROP rebuild the clause through `quote_identifier` (`MixedCaseNamedConstraintQuotedConsistently`); the annotated-check DDL side also quotes.
- [x] ~~`default_expression` scans for keywords inside string literals~~ Fixed 2026-08-06: literal-aware scanning (`DefaultExpressionIgnoresKeywordsInsideLiterals`).
- [x] ~~`ADD COLUMN … NOT NULL` without default generated silently~~ Fixed 2026-08-06: `Migration` carries `warnings()`; the differ records the hazard and the CLI prints it (`AddNotNullColumnWithoutDefaultWarns`).
- [x] ~~`generate_create_table_migration` omits indexes and enum types~~ Fixed 2026-08-06: emits native-enum `CREATE TYPE` (deduplicated) before the table and `CREATE INDEX` after, with matching rollback (`CreateTableMigrationIncludesEnumTypesAndIndexes`); enum-value additions on native enums now diff to `ALTER TYPE … ADD VALUE` and removals are an explicit error (`NativeEnumValueAdditionDiffsToAlterType`, `NativeEnumValueRemovalIsAnError`).
- [x] ~~Native enum PG type names: reserved words break~~ Partially fixed 2026-08-06: names go through `quote_identifier` (enum class Order works). Deliberately NOT namespace-qualified — versioned schema types (v1::Status vs v2::Status) must map to the same DB type; same-named enums in different namespaces remain the caller's to disambiguate (documented on `pg_enum_type_name`).
- [x] ~~Member-level non-modifier annotations silently dropped; struct-level `ann::pk` ignored~~ Fixed 2026-08-06: `constraint_diagnostics` rejects table-level annotations on members and column modifiers at struct level (compile-fail: `table_annotation_on_member`, `column_annotation_on_struct`).
- [x] ~~`string_default` doesn't escape quotes~~ Fixed 2026-08-06: embedded quotes doubled, matching `column_traits<std::string>::to_sql_string`.
- [x] ~~`year_month_day::to_sql_string` doesn't zero-pad the year~~ Fixed 2026-08-06: `{:04}` (`DateYearZeroPadsInSqlForm`).
- [x] ~~Timestamp parser accepts negative time/offset fields; >4-digit years don't round-trip; second-granularity offsets throw~~ Fixed 2026-08-06: `parse_time_field` rejects negatives, the date part is located by separator (wide years parse), and H:M:S offsets are supported (`NegativeTimeFieldsRejected`, `SecondGranularityOffsetsParse`, `WideYearsRoundTrip`).
- [x] ~~Empty `in()` list renders `IN ()`~~ Fixed 2026-08-06: empty lists render the constant `1 = 0` with no parameters, in both `TypedInCondition` and `InCondition` (`EmptyInListIsConstantFalseNotSyntaxError`).
- [x] ~~Injection/escaping surface: unit strings and runtime aliases spliced raw~~ Fixed 2026-08-06: `extract`/`date_trunc` units validated (lowercase field names), `interval()` bodies validated (quotes rejected), `date_diff` units validated, and both `AliasedColumn` and `TypedAlias` render their alias through `quote_identifier` — which also fixes mixed-case aliases folding and breaking by-name DTO matching (`MixedCaseAliasQuotedSoItSurvivesFolding`).
- [x] ~~Pool: off-mutex slot-release `notify_one()` (lost wakeup); idle teardown under `pool_mutex_`; `initial_size <= max_size` unvalidated~~ Fixed 2026-08-06: all counter releases + notifies happen under the mutex, dropped connections destruct outside the lock, and `initialize()` rejects invalid sizing (`InitialSizeExceedingMaxSizeIsError`).
- [x] ~~Async paths call blocking `PQgetResult` drains when abandoning a stream~~ Mitigated 2026-08-06: both sources send `PQcancel` before the destructor-path drain, so the wait is bounded by query abort rather than by the remaining result size.
- [x] ~~Async `start_query` failure leaves the query in flight; retries re-send~~ Fixed 2026-08-06: any start failure marks the stream finished and records `last_error`; `initialize()` refuses to re-send.
- [x] ~~Sync prepared statements: no name registry; DEALLOCATE swallowed in aborted transactions~~ Partially addressed 2026-08-06: duplicate names surface as real server errors through the `ConnectionResult` path (no silent behavior); pooled-connection `DISCARD ALL` remains under hardening features.
- [x] ~~`TransactionGuard` discards `sql_state`~~ Fixed 2026-08-06: `TransactionException::error()` carries the full `ConnectionError`, so retry loops can use `is_serialization_failure()`/`is_deadlock()` through the guard. (The re-export is into `relx`, not the global namespace.)
- [x] ~~Async `execute_raw` uses `PQsendQueryParams` unconditionally~~ Fixed 2026-08-06: paramless queries go through `PQsendQuery` (multi-statement DDL parity with sync), and `get_query_result` keeps the LAST result so an error in a later statement isn't masked.
- [x] ~~`columns_holder` doesn't walk base classes~~ Fixed 2026-08-06: uses `refl::member_array` like every other path; inherited columns qualify with the derived table's name (`BaseClassColumnsAppearInTableObjectAndDdl`).
- [x] ~~Free `when(column,…)`/`else_(column)` overloads cannot compile~~ Deleted 2026-08-06.
- [x] ~~Differ constraint classification by substring; FK name collisions; dead `constraint_mappings`/`RenameConstraintOperation`~~ Fixed 2026-08-06: classification is structural (starts_with, after name-prefix strip — `CheckContainingUniqueKeywordClassifiesAsCheck`), generated-name collisions get a content-hash suffix, and the dead API was deleted.
- [x] ~~CLI: exceptions through the exit-code API; no stream-state check; unknown arguments ignored~~ Fixed 2026-08-06: `run_migration_tool` catches and returns 1, the writer flushes and verifies stream state, and trailing arguments are parsed strictly (`--ouput` is now an error).
- [x] ~~FK actions before `references` overwritten~~ Fixed 2026-08-06: actions are collected and attached to the FOREIGN KEY/REFERENCES clause regardless of listing order (hoisted and inline DDL paths).
- [x] Misc small — fixed 2026-08-06: `_fs` literal now uses the class-NTTP form (was uninvocable); `system_user` added to the reserved list; `char`-like DTO fields rejected with a clear static_assert; `get<unsigned>` and all `Cell` numerics parse via strict `from_chars` (locale-independent, no wraparound, `long double` included); `Cell::null()` carries no fabricated text; `result.hpp` includes fixed and the broken `get_column_name<MemberPtr>()` deleted; uuid trait round-trips its quoted form and throws `std::invalid_argument`; `convert_bytea_` is reachable via `set_convert_bytea()` (live-PG opt-in test); `static_pg_sql` mirrors the runtime grammar for comments and `??`; `Value<time_point>`/`Value<year_month_day>` bind typed with binary payloads; `select()` with zero columns is a compile error; INSERT `values()` arity is checked against `columns()`; a second `update().set()` on the same column is a compile error. Remaining as documented behavior: `from(a,b).join(c)` binds the join to the last FROM entry (SQL's own semantics), no NAMEDATALEN length diagnostic.

## Test & CI gaps (round 2)

- [x] ~~compile_fail harness false pass~~ Fixed 2026-08-06: the invocation carries the PostgreSQL include dirs, every case declares its intended diagnostic via `// expect-error:` (enforced with `PASS_REGULAR_EXPRESSION` — any-other-failure now fails the test), the control file includes the same umbrella headers, and ten new cases were added (type mismatch, arithmetic on string/bool, string_view/char DTO fields, zero-column select, duplicate SET, INSERT arity, annotation placement × 2). 25/25 green.
- [x] ~~`postgresql_streaming_test.cpp` GTEST_SKIP on connect~~ Fixed 2026-08-06: hard ASSERT.
- [x] ~~`postgresql_integration_test.cpp` never compiled~~ Registered 2026-08-06.
- [x] ~~`async_dto_mapping_test.cpp` discards the connect result~~ Fixed 2026-08-06: throws through `run_test` on failure.
- [x] ~~Pipe-format round-trip property test~~ Added 2026-08-06 (`pipe_format_roundtrip_test.cpp`), splitters deduplicated into `text_format::split_cells`.
- [x] ~~Binary decoders never triggered~~ Fixed 2026-08-06: fixed-vector tests for NUMERIC base-10000 (positive/negative/zero), UUID formatting, and int2/int4/int8 sign extension; the fuzz target now asserts on its results.
- [x] ~~`TransactionGuard`: zero tests~~ Fixed 2026-08-06: ten fake-connection tests (commit/rollback ordering, double-commit, failed begin/commit with sql_state, swallowed destructor rollback, move semantics, `with_transaction`).
- [x] ~~`type_safety_test.cpp` asserts nothing~~ Rewritten 2026-08-06: positive cases assert exact SQL and parameters; negative cases live in the compile-fail suite (hard static_asserts are not SFINAE-detectable, so `static_assert(!requires{…})` cannot express them).
- [x] ~~Missing compile_fail cases~~ Ten added (see harness item). Not added: case-arm mismatch, unaliased aggregate, stateful column, unsupported column type.
- [ ] **Missing check, not missing test**: selecting a column from a table absent from FROM has no compile-time enforcement — the library's core premise, unimplemented. (Feature-scale work; still open.)
- [ ] Weak assertions: the CLI byte-identical tests, fractional-second `to_time_t` comparisons, and `TestErrorHandling` remain; the coalesce/arithmetic commented-out tests were re-enabled with real assertions.
- [x] ~~No migration test mixes column + constraint changes; no enum-column diff~~ Added 2026-08-06 (phase ordering, enum add/remove, hoisted enum CHECK, ModifyColumn ordering/USING). Still open: executing generated DDL against live PG, rollback round-trip execution.
- [ ] Untested public API: `returning_all()`, `UpdateQuery::where_in`, `autoincrement`/`identity` options, `drop_table::restrict()`, `AliasedColumn` comparisons, `query/literals.hpp`, `reflect.hpp`, `write_migration_to_file`. (`uuid_traits.hpp` now has a test file.)
- [ ] Fuzz additions: pipe-text round-trip landed as a seeded property test; bytea differential, conninfo quoting, `quote_identifier`, and chrono template-mutation fuzz remain.
