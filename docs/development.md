# relx Development Guide

Building relx from source, running the test suite, and the toolchain constraints that come with
C++26 reflection.

## Toolchain

relx is written against C++26 reflection (P2996 annotations via P3394). That narrows the toolchain
hard:

- **GCC 16.1+ with `-freflection`** — the only released compiler implementing P2996. Clang is
  expected to follow around Clang 24; MSVC has no announced support. `CMakeLists.txt` rejects
  anything else at configure time.
- **CMake 3.25+** — needed for `cxx_std_26`.
- **Boost** (headers plus `Boost::system`), and **libpq** for the PostgreSQL client.
- **Docker** — both for the GCC 16 toolchain image and for the PostgreSQL instance the integration
  tests run against.

`-freflection` is attached to the `relx` interface target, so anything linking `relx::relx` inherits
it. libstdc++'s `<meta>` is silently empty without the flag, so direct compiler invocations (scratch
programs, one-off syntax checks) must pass it themselves.

There is no Homebrew GCC 16, so **on macOS all building happens inside the container**. The host is
used for editing, `clang-format`, and running the PostgreSQL container.

## Development Container

`docker-dev/Dockerfile` pins `gcc:16` plus the build dependencies:

```bash
docker build -t relx-gcc16-dev docker-dev
```

Dependencies come from apt inside the image — `libboost-dev`, `libboost-system-dev`, `libpq-dev`.
Not `libboost-all-dev`: it drags in openmpi/gfortran, whose postinst conflicts with the `gcc:16`
image's `/usr/local` toolchain.

Configure and build into `build-gcc16` (kept separate from any host `build/` directory):

```bash
docker run --rm -v "$PWD":/repo -w /repo relx-gcc16-dev \
  bash -c 'cmake -B build-gcc16 -G Ninja -DRELX_DEV_MODE=ON && cmake --build build-gcc16 -j'
```

`RELX_DEV_MODE=ON` turns on tests, the PostgreSQL client, and install targets in one switch.

To syntax-check a single file without a full build:

```bash
docker run --rm -v "$PWD":/repo -w /repo relx-gcc16-dev \
  g++ -std=gnu++26 -freflection -fsyntax-only -Iinclude -Itest path/to/file.cpp
```

## Running Tests

The integration tests need PostgreSQL. Start it on the host (Docker Compose maps it to port 5434,
database `relx_test`):

```bash
make postgres-up      # make postgres-down / postgres-logs / postgres-clean
```

The test binary runs inside the build container, which reaches the host's PostgreSQL by forwarding
5434:

```bash
docker run --rm --add-host=host.docker.internal:host-gateway -v "$PWD":/repo -w /repo relx-gcc16-dev \
  bash -c 'socat TCP-LISTEN:5434,fork,reuseaddr TCP:host.docker.internal:5434 & sleep 1; ./build-gcc16/test/relx_tests'
```

The unit tests (schema, query building, result parsing) need no database and run without the socat
forward.

`make build` drives the same CMake configuration against a `build/` directory; it needs GCC 16.1 as
the default compiler, so run it inside the container, not on a macOS host. `make test` additionally
starts the PostgreSQL container through Docker Compose, which is a host-side operation — hence the
split above: `make postgres-up` on the host, the test binary in the container.

## CMake Options

- `RELX_DEV_MODE` — tests + PostgreSQL client + install targets (default: OFF)
- `RELX_ENABLE_TESTS` (default: OFF)
- `RELX_ENABLE_POSTGRES_CLIENT` (default: OFF)
- `RELX_ENABLE_INSTALL` (default: OFF)
- `RELX_ENABLE_COVERAGE` — coverage instrumentation, driving `make coverage` (default: OFF)

## Code Style

`make format` / `make format-check` run clang-format 20, which is fine for ordinary code but cannot
parse reflection syntax — `^^`, `[: :]`, `template for`, `[[=...]]`. Files with annotations wrap
those regions:

```cpp
// clang-format off
struct [[=relx::table("users")]] Users {
  [[=relx::ann::pk]] int id;
};
// clang-format on
```

**clang-tidy is unusable on this codebase.** No released Clang can parse P2996, so `make tidy`
fails on any header that uses reflection. It becomes usable again when Clang ships reflection
(~Clang 24).

Beyond formatting: use concepts rather than SFINAE, prefer compile-time evaluation, and document
public APIs with Doxygen comments (see [CONTRIBUTING.md](../CONTRIBUTING.md)).

## Installing relx

The recommended integration is CMake FetchContent — see the [README](../README.md). To install
system-wide instead, build in the container and install from there:

```bash
cmake -B build-gcc16 -DCMAKE_BUILD_TYPE=Release \
  -DRELX_ENABLE_INSTALL=ON -DRELX_ENABLE_POSTGRES_CLIENT=ON
cmake --build build-gcc16 -j
cmake --install build-gcc16
```

```cmake
find_package(relx REQUIRED)
target_link_libraries(my_app relx::relx)
```

The core is header-only, so copying `include/relx/` into a project also works — but then
`-freflection` and `-std=c++26` have to be set by hand, along with links to Boost.System and libpq.
