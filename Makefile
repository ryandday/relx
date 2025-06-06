# Project configuration
BUILD_DIR := build
BUILD_TYPE ?= Debug

# Default build using system dependencies
.PHONY: build
build: configure
	cd $(BUILD_DIR) && make -j

.PHONY: build-dev
build-debug: configure
	cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Debug -DRELX_DEV_MODE=ON
	cd $(BUILD_DIR) && make -j

.PHONY: build-release
build-release: configure
	cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release
	cd $(BUILD_DIR) && make -j

.PHONY: configure
configure:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

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

.PHONY: tidy
tidy:
	@echo "Running clang-tidy on source files..."
	cd $(BUILD_DIR) && find ../src ../include -name "*.cpp" -o -name "*.hpp" | \
			xargs -n1 -P$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) -I{} \
			clang-tidy -p . --header-filter='.*/(include|test)/.*\.(h|hpp)$$' --fix {}; \
	else \
		cd $(BUILD_DIR) && find ../src ../include -name "*.cpp" -o -name "*.hpp" | \
			xargs -n1 -P$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) -I{} \
			clang-tidy -p . --header-filter='.*/(include|test)/.*\.(h|hpp)$$' {}; \
	fi

.PHONY: postgres-up
postgres-up:
	docker-compose up -d postgres
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
	@echo ""
	@echo "Build targets:"
	@echo "  build            - Build the project"
	@echo "  build-debug      - Build the project in debug mode"
	@echo "  build-release    - Build the project in release mode"
	@echo ""
	@echo "Configuration targets:"
	@echo "  configure        - Configure project with CMake"
	@echo ""
	@echo "Build and test:"
	@echo "  clean            - Remove build directory"
	@echo "  test             - Build and run the tests with CTest (starts PostgreSQL)"
	@echo ""
	@echo "Code quality:"
	@echo "  format           - Format code using clang-format"
	@echo "  format-check     - Check code formatting"
	@echo "  tidy             - Run clang-tidy on source files"
	@echo ""
	@echo "PostgreSQL for testing:"
	@echo "  postgres-up      - Start PostgreSQL container"
	@echo "  postgres-down    - Stop PostgreSQL container"
	@echo "  postgres-logs    - Show PostgreSQL container logs"
	@echo "  postgres-clean   - Remove PostgreSQL container and volumes"