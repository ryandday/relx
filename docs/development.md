---
layout: default
title: Development Guide
nav_order: 8
---

# relx Development Guide

This guide covers building relx from source, running tests, alternative installation methods, and contributing to the project.

## Alternative Installation Methods

### System-Wide Installation

```bash
# Clone and build
git clone https://github.com/ryandday/relx.git
cd relx

# Install dependencies (choose one method below)

# Method A: Using system package manager (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install libboost-dev libpq-dev

# Method B: Using system package manager (macOS with Homebrew)
brew install boost postgresql

# Method C: Using Conan (cross-platform)
pip install conan
conan profile detect --force
conan install . --build=missing

# Configure and build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DRELX_ENABLE_INSTALL=ON -DRELX_ENABLE_POSTGRES_CLIENT=ON
make -j
sudo make install
```

Then in your project's `CMakeLists.txt`:

```cmake
find_package(relx REQUIRED)
target_link_libraries(my_app relx::relx)
```

### Manual Integration

Since relx is header-only, you can also manually integrate it:

1. Copy the `include/relx/` directory to your project
2. Add the include path to your compiler flags
3. Link against Boost (system, thread) and PostgreSQL client library

## Building and Testing relx (For Development)

### Prerequisites

- C++23 bleeding edge compiler (GCC 15, Clang 20)
- CMake 3.15 or later
- Docker (for running PostgreSQL during tests)
- One of the following for dependencies:
  - System package manager with Boost and PostgreSQL development libraries
  - Conan package manager

### Build Steps

```bash
# Clone the repository
git clone https://github.com/ryandday/relx.git
cd relx

# Install dependencies using your preferred method:

# Option A: System packages (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install libboost-dev libpq-dev cmake build-essential

# Option B: System packages (macOS with Homebrew)
brew install boost postgresql cmake

# Option C: Conan (cross-platform)
pip install conan
conan profile detect --force
conan install . --build=missing

# Build the project (includes tests and examples)
make build

# Or manually with cmake:
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DRELX_DEV_MODE=ON
make -j
```

### Running Tests

```bash
# Run all tests (automatically starts PostgreSQL container)
make test

# Or run tests manually:
make postgres-up    # Start PostgreSQL container
cd build/test && ./relx_tests
make postgres-down  # Stop PostgreSQL container when done
```

### Development Commands

```bash
# Clean build directory
make clean

# Format code
make format

# Check code formatting
make format-check

# Run static analysis
make tidy

# Build release version
make build-release

# PostgreSQL container management
make postgres-up     # Start PostgreSQL
make postgres-down   # Stop PostgreSQL  
make postgres-logs   # View PostgreSQL logs
make postgres-clean  # Remove PostgreSQL container and data

# View all available make targets
make help
```

## CMake Configuration Options

When building relx, you can configure these CMake options:

- `RELX_ENABLE_POSTGRES_CLIENT=ON/OFF` - Enable PostgreSQL client library (default: OFF)
- `RELX_ENABLE_TESTS=ON/OFF` - Enable building tests (default: OFF) 
- `RELX_ENABLE_INSTALL=ON/OFF` - Enable installation targets (default: OFF)
- `RELX_DEV_MODE=ON/OFF` - Enable development mode (automatically enables tests, postgres client, and install targets) (default: OFF)

Example:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DRELX_ENABLE_POSTGRES_CLIENT=ON -DRELX_ENABLE_TESTS=ON
```

## Contributing

### Code Style

relx follows specific C++ coding standards. Before submitting contributions:

1. Run `make format` to format your code
2. Run `make format-check` to verify formatting
3. Run `make tidy` for static analysis
4. Ensure all tests pass with `make test`

### Coding Standards

- Use C++23 features
- Follow the project's naming conventions (see main README)
- Use concepts instead of SFINAE
- Prefer compile-time evaluation where possible
- Write comprehensive tests for new features

See the project's `.clang-format` and `.clang-tidy` configuration files for detailed style requirements.

### Docker Development Environment

The project includes Docker Compose configuration for development:

```bash
# Start PostgreSQL for testing
make postgres-up

# View PostgreSQL logs
make postgres-logs

# Stop PostgreSQL
make postgres-down

# Clean PostgreSQL data
make postgres-clean
```

The tests automatically manage the PostgreSQL container, so you typically don't need to start it manually. 