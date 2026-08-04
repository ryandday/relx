# relx CRUD server

Async CRUD backend spike combining:

- **Boost.Beast** — HTTP transport, C++20 coroutine handlers on `asio::awaitable`
- **glaze** — JSON (de)serialization directly on plain aggregates
- **relx** — schema, type-safe queries, PostgreSQL access

One annotated struct (`src/schema.hpp`) is the SQL schema, the query result
type, and the JSON response shape. Blocking libpq work runs on a dedicated
thread pool behind an awaitable facade (`src/db.hpp`); when relx grows a native
async pool it drops in behind the same `co_await db.run(...)` contract.

## Build (gcc:16 container)

```bash
# from the relx repo root
docker compose up -d postgres
docker run --rm -v $(pwd):/repo -w /repo relx-gcc16-dev \
  bash -c 'cmake -B crud-server/build -S crud-server -G Ninja -DCMAKE_BUILD_TYPE=Release \
           && cmake --build crud-server/build -j'
```

## Run

```bash
docker run --rm -p 8080:8080 --add-host=host.docker.internal:host-gateway \
  -e DB_HOST=host.docker.internal -v $(pwd):/repo -w /repo relx-gcc16-dev \
  ./crud-server/build/crud_server
```

Environment: `PORT` (8080), `DB_HOST` (localhost), `DB_PORT` (5434),
`DB_NAME` (relx_test), `DB_USER`/`DB_PASSWORD` (postgres).

## Endpoints

| Method | Path | Body | Response |
|---|---|---|---|
| GET | /users | — | 200 `[User]` |
| GET | /users/:id | — | 200 `User`, 404 |
| POST | /users | `{name, email, bio?}` | 201 `User`, 400, 409 |
| PUT | /users/:id | `{name, email, bio?}` | 200 `User`, 404 |
| DELETE | /users/:id | — | 204, 404 |

## Breaking out into its own repo

The only tie to the parent checkout is `RELX_SOURCE_DIR` (defaults to `..`).
Point it elsewhere, or replace the `add_subdirectory` block in CMakeLists.txt
with a `FetchContent_Declare(relx GIT_REPOSITORY ...)`.
