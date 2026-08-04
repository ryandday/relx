# relx events API

Acceptance example for **relx::web**: the events feature of a real social-app
backend (users, JWT auth, owned events, invites) built on the relx stack —
Boost.Beast coroutine HTTP, glaze JSON, relx schema/queries, PostgreSQL.

Three annotated structs (`src/schema.hpp`) are the SQL schema, the query result
types, and the JSON shapes. Request DTOs are hand-written and compile-time
checked against their tables via `[[=relx::web::projects<T>]]` — renaming a
column breaks the build at every stale DTO. Policy (ownership, validation,
invite rules) is plain code in handlers (`src/handlers.hpp`); the mechanics
(transport, JSON, auth plumbing, pagination, 404 helpers) come from relx::web.

## Build & run (gcc:16 container)

```bash
# from the relx repo root
docker compose up -d postgres
docker run --rm -v $(pwd):/repo -w /repo relx-gcc16-dev \
  bash -c 'cmake -B crud-server/build -S crud-server -G Ninja -DCMAKE_BUILD_TYPE=Release \
           && cmake --build crud-server/build -j'
docker run --rm -p 8090:8080 --add-host=host.docker.internal:host-gateway \
  -e DB_HOST=host.docker.internal -v $(pwd):/repo -w /repo relx-gcc16-dev \
  ./crud-server/build/crud_server
```

Environment: `PORT` (8080 in-container), `JWT_SECRET`, `DB_HOST`/`DB_PORT`/
`DB_NAME`/`DB_USER`/`DB_PASSWORD` (defaults target the compose postgres).

## API

| Method | Path | Auth | Notes |
|---|---|---|---|
| POST | /auth/signup | — | `{name, email}` → 201 `{token, user}`, 409 dup email |
| POST | /auth/token | — | `{email}` → `{token, user}` |
| GET | /users/me | ✓ | current user |
| GET | /events?limit&offset | ✓ | own events, by start_time |
| GET | /events/invited | ✓ | events shared with you (join) |
| POST | /events | ✓ | `{title, start_time, end_time?, description?, location?, is_all_day?}`; 422 on empty title / end before start |
| GET | /events/:id | ✓ | owner or invitee; others 404 (no existence leak) |
| PUT | /events/:id | ✓ | owner only (404 otherwise), full replace |
| DELETE | /events/:id | ✓ | owner only, clears invites |
| POST | /events/:id/invites | ✓ | owner only; 409 duplicate/self, 404 unknown user |

## Quick tour

```bash
B=http://localhost:8090
TOKEN=$(curl -s -X POST $B/auth/signup -H 'Content-Type: application/json' \
  -d '{"name":"Ada","email":"ada@example.com"}' | python3 -c 'import json,sys;print(json.load(sys.stdin)["token"])')
curl -s -X POST $B/events -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' \
  -d '{"title":"Standup","start_time":"2026-09-01T09:00:00Z"}'
curl -s "$B/events?limit=10" -H "Authorization: Bearer $TOKEN"
```

## Breaking out into its own repo

The only tie to the parent checkout is `RELX_SOURCE_DIR` (defaults to `..`).
Point it elsewhere, or replace the `add_subdirectory` block in CMakeLists.txt
with a `FetchContent_Declare(relx GIT_REPOSITORY ...)`.
