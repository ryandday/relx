# Contributing to relx

Thank you for your interest in contributing to relx! This document provides guidelines for contributing to the project.

## Development Setup

1. **Clone the repository:**
   ```bash
   git clone https://github.com/ryandday/relx.git
   cd relx
   ```

2. **Build the toolchain image.** relx needs GCC 16.1 for C++26 reflection, which no
   package manager ships on macOS, so building happens inside the container:
   ```bash
   docker build -t relx-gcc16-dev docker-dev
   ```

3. **Build the project:**
   ```bash
   docker run --rm -v "$PWD":/repo -w /repo relx-gcc16-dev \
     bash -c 'cmake -B build-gcc16 -G Ninja -DRELX_DEV_MODE=ON && cmake --build build-gcc16 -j'
   ```

4. **Run tests** (PostgreSQL runs on the host via `make postgres-up`):
   ```bash
   make postgres-up
   docker run --rm --add-host=host.docker.internal:host-gateway -v "$PWD":/repo -w /repo relx-gcc16-dev \
     bash -c 'socat TCP-LISTEN:5434,fork,reuseaddr TCP:host.docker.internal:5434 & sleep 1; ./build-gcc16/test/relx_tests'
   ```

See [docs/development.md](docs/development.md) for the full toolchain notes.

### Documentation Standards

When contributing code, please ensure:

1. **All public APIs are documented** with Doxygen-style comments:
   ```cpp
   /// @brief Brief description of the function
   /// @param param_name Description of the parameter
   /// @return Description of what the function returns
   /// @throws ExceptionType When this exception is thrown
   template <typename T>
   auto my_function(const T& param) -> ReturnType;
   ```

2. **Use examples in documentation:**
   ```cpp
   /**
    * @brief Create a SELECT query
    *
    * @example
    * ```cpp
    * constexpr auto users = relx::t<Users>;
    * auto query = relx::select(users.id, users.name)
    *     .from(users)
    *     .where(users.age > 18);
    * ```
    */
   ```

3. **Update user guides** in the `docs/` directory when adding new features.

## Code Style

- Use the provided `.clang-format` configuration; run `make format` before submitting PRs
- Wrap reflection-heavy regions in `// clang-format off` / `// clang-format on` — clang-format 20
  cannot parse `^^`, `[: :]`, `template for`, or `[[=...]]`
- `make tidy` does **not** work: no released clang-tidy can parse P2996, so it fails on any header
  using reflection. It becomes usable when Clang ships reflection (~Clang 24)

## Testing

- Add tests for all new functionality
- Ensure existing tests pass (see Development Setup above for the container invocation)
- Test coverage can be generated with `make coverage` from inside the container

## Documentation

### Generating Documentation Locally

The project uses **Doxygen** to auto-generate API documentation from source code comments.

```bash
# Generate documentation
make docs

# Generate and open documentation in browser
make docs-open

# Clean generated documentation
make docs-clean
```

## Pull Request Process

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Make your changes following the style guidelines
4. Add/update tests and documentation  
5. Ensure all tests pass
6. Format code: `make format`
7. Submit a pull request with a clear description

## Questions?

Feel free to open an issue for questions, bug reports, or feature requests. 