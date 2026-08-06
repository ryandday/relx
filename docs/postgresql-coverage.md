# PostgreSQL Feature Coverage

What relx supports from the PostgreSQL manual, and what it doesn't yet. relx targets PostgreSQL only. Organized by manual chapter. Verified against the codebase on branch `cpp26-reflection` (2026-08-05).

Legend: ✅ supported · 🟡 partial · ❌ not yet · ⛔ deliberately out of scope

The escape hatch for anything ❌ is `Connection::execute_raw(sql, params)` — the full SQL surface is always reachable as raw text.

## Queries — SELECT (manual ch. 7, `SELECT` reference)

| Feature | Status | Notes |
|---|---|---|
| Select list, expressions, aliases (`AS`) | ✅ | Runtime `as()` plus compile-time typed `as<"name">` feeding `row_type_for` |
| `SELECT *` | ⛔ | `select_all` expands to an explicit column list via reflection — deliberate |
| `DISTINCT` | ✅ | `select_distinct`, `count_distinct`, `distinct(expr)` |
| `DISTINCT ON (...)` | ❌ | |
| Multi-table `FROM` | ✅ | Chained/variadic `from()` |
| Table aliases (`FROM users AS u`) | ✅ | `relx::t<Users, "u">` — alias in the type; self-joins via two aliases |
| Subquery in `FROM` | ❌ | `from()` only accepts schema tables |
| `LATERAL` | ❌ | |
| `JOIN` (inner/left/right/full/cross) | ✅ | `ON` conditions with bound params; multiple joins |
| `USING (...)`, `NATURAL JOIN` | ❌ | |
| Comparison ops, `AND`/`OR`/`NOT` | ✅ | |
| `IN (list)` / `IN (subquery)` | ✅ | `NOT IN` only as `!in(...)` |
| `= ANY(array param)` | ✅ | `in_any()` — one array bind param, size-independent SQL, empty list legal |
| `ANY`/`SOME`/`ALL (subquery)` | ❌ | |
| `EXISTS` / correlated subqueries | ✅ | |
| Scalar subquery in select list / comparison | ❌ | Compiles but emits unparenthesized (invalid) SQL |
| `BETWEEN` | 🟡 | Bounds bind as strings only |
| `LIKE` | ✅ | `NOT LIKE` via `!like(...)` |
| `ILIKE`, `SIMILAR TO`, regex `~` | ❌ | |
| `IS NULL` / `IS NOT NULL` | ✅ | |
| `IS [NOT] DISTINCT FROM` | ❌ | |
| `GROUP BY` / `HAVING` | ✅ | Columns and expressions |
| `GROUPING SETS` / `ROLLUP` / `CUBE` | ❌ | |
| Window functions (`OVER`, frames, `WINDOW`) | ❌ | No clause slot exists on `SelectQuery` |
| `ORDER BY` (asc/desc, expressions, multi-key) | ✅ | Bool columns rejected at compile time |
| `NULLS FIRST/LAST` | ❌ | |
| `LIMIT` / `OFFSET` | ✅ | Emitted as bound params |
| `FETCH FIRST ... WITH TIES` | ❌ | |
| `UNION` / `INTERSECT` / `EXCEPT` | ❌ | |
| CTEs (`WITH`, `WITH RECURSIVE`) | ❌ | |
| `CASE WHEN` | ✅ | Searched form only; branch types checked at compile time |
| `COALESCE` | ✅ | `NULLIF`, `GREATEST`, `LEAST` ❌ |
| Locking (`FOR UPDATE`/`SHARE`, `SKIP LOCKED`, `NOWAIT`) | ❌ | |
| Standalone `VALUES`, `TABLESAMPLE` | ❌ | |

## Data manipulation (`INSERT`, `UPDATE`, `DELETE`, `MERGE`, `COPY`, `TRUNCATE`)

| Feature | Status | Notes |
|---|---|---|
| `INSERT` single/multi-row | ✅ | With or without explicit column list |
| `INSERT ... SELECT` | ✅ | |
| `DEFAULT VALUES` / per-column `DEFAULT` | ❌ | `DEFAULT` exists only at DDL level |
| `ON CONFLICT` (upsert) | ✅ | `.upsert()` derives the conflict target from the pk annotation; DO UPDATE from EXCLUDED, DO NOTHING when only key columns are inserted |
| `RETURNING` | ✅ | On insert/update/delete; `returning_all()` reflection-expanded |
| `UPDATE ... SET` expressions | ✅ | Literals, arithmetic, functions, CASE; repeated `where()` replaces (combine with `&&`) |
| `UPDATE ... FROM` | ❌ | |
| `DELETE` with/without `WHERE` | ✅ | |
| `DELETE ... USING` | ❌ | |
| `MERGE` | ❌ | |
| `COPY` | ❌ | Protocol statuses explicitly rejected at the connection layer |
| `TRUNCATE` | ❌ | |

## Data types (manual ch. 8)

Mapping is `schema::column_traits<T>`; users can specialize it for anything missing. `std::optional<T>` is the only nullability mechanism.

| PostgreSQL type | Status | C++ type |
|---|---|---|
| `INTEGER`, `BIGINT` | ✅ | `int`, `long`/`long long` |
| `SMALLINT` | ❌ | No mapping (binds fine as a param: OID int2) |
| `REAL`, `DOUBLE PRECISION` | ✅ | `float`, `double` |
| `NUMERIC` / `DECIMAL` | 🟡 | No column mapping; binary result decode exists (base-10000 decoder) |
| `BOOLEAN` | ✅ | `bool` |
| `TEXT` | ✅ | `std::string` |
| `VARCHAR(n)` / `CHAR(n)` | ❌ | No length-parameterized string types |
| Enums | ✅ | Any C++ enum → `TEXT` + `CHECK`; `ann::native_enum` → `CREATE TYPE ... AS ENUM` (creation SQL generated, not auto-wired into `create_table`) |
| `TIMESTAMPTZ` | ✅ | `std::chrono::system_clock::time_point`, binary bind |
| `TIMESTAMP` (no tz), `TIME`, `INTERVAL` | ❌ | Interval exists only as a query-side literal |
| `DATE` | ✅ | `std::chrono::year_month_day`, binary bind |
| `UUID` | ✅ | `boost::uuids::uuid`, binary bind/decode (OID 2950) |
| `BYTEA` | 🟡 | Hex decode on results; no column type, no binary bind |
| `JSON` / `JSONB` | ✅ | Struct-typed JSONB columns via `[[=relx::ann::jsonb]]`; reflection-derived strict encode/decode |
| Array columns | 🟡 | No column type; array *bind params* fully supported (bool/int2/int4/int8/float4/float8/text) |
| Identity | ✅ | `ann::autoincrement` → `GENERATED ALWAYS AS IDENTITY`; `identity<...>` options. `BY DEFAULT` form ❌; `SERIAL` ⛔ |
| `MONEY`, `INET`/`CIDR`/`MACADDR`, geometric, `TSVECTOR`, ranges, composites, domains | ❌ | |

Hazard: unspecialized integral types (`short`, `unsigned`, `std::size_t`) silently fall through to the primary template's `TEXT`.

## Functions and operators (manual ch. 9)

| Category | Status | Notes |
|---|---|---|
| Comparison, logical | ✅ | |
| Arithmetic `+ - * /` | ✅ | Column/value/expr combinations; `%`, `^`, bitwise, unary minus ❌ |
| String | 🟡 | `lower`, `upper`, `length`, `trim` only. No `\|\|`/`concat`, `substring`, `replace`, `position`, `regexp_*`, `to_char`, ... |
| Math | 🟡 | `abs` only. No `round`, `ceil`, `floor`, `power`, `sqrt`, `mod`, `random`, ... |
| Date/time | ✅ | Richest area: `extract`, `date_trunc`, `age` (via `date_diff`), intervals with `+`/`-` operators, `now`/`current_*`, many convenience wrappers |
| Aggregates | 🟡 | `count`, `count_distinct`, `sum`, `avg`, `min`, `max` with compile-time type guards. No `string_agg`, `array_agg`, `bool_and/or`, `json_agg`, statistics, ordered-set. No `FILTER`, no `ORDER BY` inside aggregates |
| Window functions | ❌ | See Queries |
| Casts (`::`, `CAST`) | ❌ | Only hard-coded inside `date_diff` internals |
| JSON/JSONB operators & functions | ❌ | |
| Array operators & functions | ❌ | Beyond `= ANY(param)` |
| Full-text search | ❌ | |
| Range operators | ❌ | |
| UUID generation (`gen_random_uuid`) | 🟡 | Only as DDL default via `default_sql<"gen_random_uuid()">`; no query-side wrapper |
| Sequence functions (`nextval`, ...) | ❌ | |
| Conditional (`CASE`, `COALESCE`) | ✅ | `NULLIF`, `GREATEST`/`LEAST` ❌ |
| Arbitrary function escape hatch | 🟡 | `FunctionExpr`/`NullaryFunctionExpr` take a runtime name but only nullary/unary; no raw-SQL expression node |

## DDL (manual ch. 5, `CREATE TABLE`/`CREATE INDEX` reference)

| Feature | Status | Notes |
|---|---|---|
| `CREATE TABLE` | ✅ | Consteval (`create_table_sql<T>`) and runtime builders; `IF NOT EXISTS` |
| Primary key (single, composite) | ✅ | `ann::pk`, `ann::composite_pk` — compile-time validated |
| Foreign keys | ✅ | `ann::fk<^^T::col>`, composite FK with target-PK verification, self-ref via `references<>`; `ON DELETE`/`ON UPDATE` as unvalidated strings |
| Unique (column, composite) | ✅ | |
| Check constraints | ✅ | Column-level `check<"expr">`, table-level `ann::check(...).named(...)` |
| Defaults | ✅ | Literal, string, `default_sql<"expr">`, NULL; float defaults disable the consteval path |
| Generated columns (`GENERATED ... STORED`) | ❌ | |
| `COLLATE`, `DEFERRABLE`, `EXCLUDE`, `NULLS NOT DISTINCT` | ❌ | |
| Indexes | 🟡 | `ann::index_on(...)`, `.unique()`, multi-column. No partial (`WHERE`), expression, `USING` method, `INCLUDE`, custom name, `CONCURRENTLY` |
| `TEMPORARY`, `UNLOGGED`, `PARTITION BY`, `INHERITS`, tablespaces | ❌ | |
| Identifier quoting | ❌ | All identifiers emitted raw |
| `DROP TABLE` | ✅ | `IF EXISTS`, `CASCADE`/`RESTRICT` |
| Views, materialized views | ❌ | |
| `CREATE SCHEMA` / `EXTENSION` / `FUNCTION` / `TRIGGER`, `COMMENT ON`, `GRANT` | ❌ | |
| `CREATE TYPE ... AS ENUM` | 🟡 | `create_enum_type_sql<E>()` generates it; not wired into table creation or migrations |

## Migrations (`ALTER TABLE` surface)

Struct-vs-struct diffing only — never against a live database. Every operation has `rollback_sql()`.

| Change | Status | Generated |
|---|---|---|
| Add / drop column | ✅ | `ADD COLUMN` / `DROP COLUMN` |
| Column rename | ✅ | Opt-in via `column_mappings`; with `column_transformations` does add+`UPDATE`+drop with reversible SQL |
| Type / nullability / default change | 🟡 | Rebuilds the column (`DROP`+`ADD` — data loss); no `ALTER COLUMN ... TYPE/SET NOT NULL/SET DEFAULT` |
| Add / drop table-level constraint | ✅ | `ADD CONSTRAINT` / `DROP CONSTRAINT`; column-level `unique` hoisted so it diffs without column rebuild |
| Column-level FK / CHECK change | 🟡 | Invisible as constraints — diffs as column rebuild |
| Add / drop index | ✅ | |
| Enum evolution | ❌ | TEXT+CHECK enums rebuild the column; native enums produce no diff and there is no `ALTER TYPE ... ADD VALUE` |
| Multi-table / schema-wide diff, table rename | ❌ | Single table only |
| Migration state tracking / runner | ❌ | Output is SQL strings; no version ledger, no executor |
| Live-database introspection | ❌ | No `information_schema`/`pg_catalog` reading |

## Transactions and concurrency (manual ch. 13)

| Feature | Status | Notes |
|---|---|---|
| `BEGIN`/`COMMIT`/`ROLLBACK` | ✅ | Sync and async; RAII `TransactionGuard` (sync only, throws) |
| Isolation levels | ✅ | All four |
| `READ ONLY` / `DEFERRABLE` characteristics | ❌ | |
| Savepoints / nested transactions | ❌ | Nested `begin` is an error |
| Row locking (`SELECT ... FOR UPDATE`) | ❌ | See Queries |
| Advisory locks | ❌ | |

## Client interface (manual libpq chapter)

| Feature | Status | Notes |
|---|---|---|
| Sync connection (libpq) | ✅ | Keyword params or raw conninfo (URI passes through to libpq) |
| Async connection | ✅ | Boost.Asio + coroutines, non-blocking libpq; separate stack, not a `Connection` subclass |
| Connection pooling | ✅ | Sync and async; validation, idle reaping, auto-rollback on return |
| Typed parameter binding | ✅ | Real type OIDs; binary encoding for bool/integers/floats/date/timestamptz/uuid/arrays; typed NULLs |
| Binary results | 🟡 | Sync only, auto-selected per query via compile-time decodability check; async hardcodes text |
| Struct mapping | ✅ | Reflection-based name matching, compile-time select-list coverage check, DTO-free `fetch_all`/`fetch_one` via synthesized row types |
| Streaming results | ✅ | `PQsetSingleRowMode` (client-side) sync + async; lazy row parsing. Text-only params; pipe-delimited internal row format can't hold cells containing `\|` or literal `NULL` |
| Prepared statements | 🟡 | Async path is correct (`PQsendPrepare`/`PQsendQueryPrepared` + cache); sync path prepares but executes via string-built `EXECUTE ...` with hand-rolled escaping. Never used automatically |
| Query cancellation (`PQcancel`) | ❌ | Only cooperative break-out of streaming loops |
| Pipeline mode / batching | ❌ | Explicitly rejected statuses |
| `COPY` protocol | ❌ | Explicitly rejected statuses |
| `LISTEN`/`NOTIFY` | ❌ | |
| Server-side cursors (`DECLARE`/`FETCH`) | ❌ | Streaming covers the forward-only case |
| Large objects | ❌ | |
| SSL/TLS | 🟡 | `sslmode`/`sslcert`/`sslkey`/`sslrootcert` pass-through; no validation, no `channel_binding`/`gssencmode`, untested |
| Rich error details (SQLSTATE, constraint names) | 🟡 | `PostgreSQLError` type exists but no execution path constructs it; `execute_raw` returns flat `ConnectionError` |
| `EXPLAIN` helpers | ❌ | |

## Biggest gaps by impact

1. **Window functions, CTEs, set operations** — whole query classes missing; no clause slots exist yet.
2. **Casts and a raw-expression escape hatch** — today the only fallback is a whole raw statement; a `raw_expr("...")` node would cover most one-off function gaps cheaply.
3. **Arrays as column types** — param binding is already ahead of the schema layer here (JSONB landed as Phase 12).
4. **SQLSTATE-aware errors** — the type exists, unwired; duplicate-key detection is the payoff.
