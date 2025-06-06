# Contributing to relx

Thank you for your interest in contributing to relx! This document provides guidelines for contributing to the project.

## Development Setup

1. **Clone the repository:**
   ```bash
   git clone https://github.com/ryandday/relx.git
   cd relx
   ```

2. **Install dependencies:**
   ```bash
   # macOS
   brew install boost postgresql doxygen

   # Ubuntu/Debian  
   sudo apt-get install libboost-all-dev libpq-dev doxygen
   ```

3. **Build the project:**
   ```bash
   make build
   ```

4. **Run tests:**
   ```bash
   make test
   ```

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
    * Users users;
    * auto query = relx::select(users.id, users.name)
    *     .from(users)
    *     .where(users.age > 18);
    * ```
    */
   ```

3. **Update user guides** in the `docs/` directory when adding new features.

## Code Style

- Use the provided `.clang-format` configuration
- Run `make format` before submitting PRs
- Ensure `make tidy` passes without errors

## Testing

- Add tests for all new functionality
- Ensure existing tests pass: `make test`
- Test coverage can be generated with: `make coverage`

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
5. Ensure all tests pass: `make test`
6. Format code: `make format`
7. Submit a pull request with a clear description

## Questions?

Feel free to open an issue for questions, bug reports, or feature requests. 