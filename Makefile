# Project configuration
BUILD_DIR := build
BUILD_TYPE ?= Debug

.PHONY: build
build: configure
	cd $(BUILD_DIR) && make -j

# Conan commands
.PHONY: conan-install
conan-install:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && conan install .. --build=missing

# Build commands
.PHONY: configure
configure: conan-install
	cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# Run the executable
.PHONY: run
run: build
	./$(BUILD_DIR)/schema_example

.PHONY: build-tests
build-tests:
	cd $(BUILD_DIR) && cmake --build . --target relx_tests --parallel 4

.PHONY: test
test: build-tests postgres-up
	cd $(BUILD_DIR) && ./relx_tests

# Development helpers
.PHONY: format
format:
	find src include -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs clang-format -i

.PHONY: format-check
format-check:
	@echo "Checking code formatting..."
	@find src include -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | \
		xargs clang-format --dry-run --Werror

.PHONY: format-diff
format-diff:
	@echo "Showing formatting differences..."
	@find src include -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | \
		xargs clang-format --dry-run --Werror --output-replacements-xml

.PHONY: tidy
tidy:
	@echo "Running clang-tidy on source files..."
	cd $(BUILD_DIR) && find ../src ../include -name "*.cpp" -o -name "*.hpp" | \
		xargs clang-tidy -p . --header-filter='.*/(include|test)/.*\.(h|hpp)$$'

.PHONY: tidy-check
tidy-check:
	@echo "Running clang-tidy checks (warnings as errors)..."
	cd $(BUILD_DIR) && find ../src ../include -name "*.cpp" -o -name "*.hpp" | \
		xargs clang-tidy -p . --header-filter='.*/include/.*\.(h|hpp)$$'

.PHONY: tidy-fix
tidy-fix:
	@echo "Running clang-tidy with automatic fixes..."
	cd $(BUILD_DIR) && find ../src ../include -name "*.cpp" -o -name "*.hpp" | \
		xargs clang-tidy -p . --header-filter='.*/(include|test)/.*\.(h|hpp)$$' --fix

.PHONY: tidy-strict
tidy-strict:
	@echo "Running clang-tidy with strict external dependency exclusion..."
	cd $(BUILD_DIR) && find ../src ../include -name "*.cpp" -o -name "*.hpp" | \
		xargs clang-tidy -p . --header-filter='.*/(include|test)/.*\.(h|hpp)$$' \
		--system-headers=false \
		--extra-arg=-isystem --extra-arg=/dev/null

.PHONY: tidy-safe
tidy-safe:
	@echo "Running clang-tidy on source files (one at a time)..."
	@cd $(BUILD_DIR) && for file in $$(find ../src ../include -name "*.cpp" -o -name "*.hpp"); do \
		echo "Processing $$file..."; \
		clang-tidy -p . --header-filter='.*/include/.*\.(h|hpp)$$' "$$file" || true; \
	done

.PHONY: tidy-single
tidy-single:
	@echo "Running clang-tidy on a single file for testing..."
	cd $(BUILD_DIR) && find ../src -name "*.cpp" | head -1 | \
		xargs clang-tidy -p . --header-filter='.*/include/.*\.(h|hpp)$$'

# Multithreaded versions for faster analysis
.PHONY: tidy-mt
tidy-mt:
	@echo "Running clang-tidy on source files (multithreaded)..."
	cd $(BUILD_DIR) && find ../src ../include -name "*.cpp" -o -name "*.hpp" | \
		xargs -n1 -P$$(sysctl -n hw.ncpu) -I{} clang-tidy -p . --header-filter='.*/include/.*\.(h|hpp)$$' {}

.PHONY: tidy-check-mt
tidy-check-mt:
	@echo "Running clang-tidy checks (multithreaded, warnings as errors)..."
	cd $(BUILD_DIR) && find ../src ../include -name "*.cpp" -o -name "*.hpp" | \
		xargs -n1 -P$$(sysctl -n hw.ncpu) -I{} clang-tidy -p . --header-filter='.*/include/.*\.(h|hpp)$$' {}

.PHONY: tidy-fix-mt
tidy-fix-mt:
	@echo "Running clang-tidy with automatic fixes (multithreaded)..."
	cd $(BUILD_DIR) && find ../src ../include -name "*.cpp" -o -name "*.hpp" | \
		xargs -n1 -P$$(sysctl -n hw.ncpu) -I{} clang-tidy -p . --header-filter='.*/include/.*\.(h|hpp)$$' --fix {}

# Docker Compose commands
.PHONY: postgres-up
postgres-up:
	docker-compose up -d postgres
	# Wait for PostgreSQL container to be healthy (includes database initialization)
	@echo "Waiting for PostgreSQL to be ready..."
	@timeout=60; \
	while [ $$timeout -gt 0 ]; do \
		if docker-compose ps postgres | grep -q "healthy"; then \
			echo "PostgreSQL is healthy!"; \
			break; \
		fi; \
		echo "Waiting... ($$timeout seconds remaining)"; \
		sleep 2; \
		timeout=$$((timeout-2)); \
	done; \
	if [ $$timeout -eq 0 ]; then \
		echo "Timeout waiting for PostgreSQL to become healthy"; \
		docker-compose logs postgres; \
		exit 1; \
	fi
	# Verify database connection
	@echo "Verifying database connection..."
	@docker-compose exec -T postgres psql -U postgres -d relx_test -c "SELECT 'Database is ready!' as status;" || \
	(echo "Failed to connect to relx_test database"; docker-compose logs postgres; exit 1)
	@echo "PostgreSQL and relx_test database are ready for testing!"

.PHONY: postgres-down
postgres-down:
	docker-compose down

.PHONY: postgres-logs
postgres-logs:
	docker-compose logs postgres

.PHONY: postgres-clean
postgres-clean:
	docker-compose down -v

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  build            - Build the project"
	@echo "  conan-install    - Install dependencies using Conan"
	@echo "  configure        - Configure project with CMake"
	@echo "  clean            - Remove build directory"
	@echo "  run              - Build and run the application"
	@echo "  test             - Build and run the tests with CTest (starts PostgreSQL)"
	@echo "  format           - Format code using clang-format"
	@echo "  format-check     - Check code formatting"
	@echo "  format-diff      - Show formatting differences"
	@echo "  tidy             - Run clang-tidy on source files"
	@echo "  tidy-check       - Run clang-tidy checks (warnings as errors)"
	@echo "  tidy-fix         - Run clang-tidy with automatic fixes"
	@echo "  tidy-strict      - Run clang-tidy with strict external dependency exclusion"
	@echo "  tidy-safe        - Run clang-tidy on source files (one at a time)"
	@echo "  tidy-single      - Run clang-tidy on a single file for testing"
	@echo "  tidy-mt          - Run clang-tidy on source files (multithreaded)"
	@echo "  tidy-check-mt    - Run clang-tidy checks (multithreaded, warnings as errors)"
	@echo "  tidy-fix-mt      - Run clang-tidy with automatic fixes (multithreaded)"
	@echo "  postgres-up      - Start PostgreSQL container"
	@echo "  postgres-down    - Stop PostgreSQL container"
	@echo "  postgres-logs    - Show PostgreSQL container logs"
	@echo "  postgres-clean   - Remove PostgreSQL container and volumes"