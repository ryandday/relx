# Project configuration
BUILD_DIR := build
BUILD_TYPE ?= Debug

# Make commands persist their working directory within each target
.ONESHELL:

.PHONY: build
build:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DRELX_DEV_MODE=ON
	cmake --build $(BUILD_DIR) -j

.PHONY: build-release
build-release: 
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) -j

.PHONY: build-coverage
build-coverage:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DRELX_DEV_MODE=ON -DRELX_ENABLE_COVERAGE=ON
	cmake --build $(BUILD_DIR) -j

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: test
test: build postgres-up
	./$(BUILD_DIR)/test/relx_tests

.PHONY: coverage
coverage: build-coverage postgres-up
	cmake --build $(BUILD_DIR) --target coverage

.PHONY: coverage-clean
coverage-clean:
	@if [ -d "$(BUILD_DIR)" ]; then \
		cmake --build $(BUILD_DIR) --target coverage-clean; \
	else \
		echo "Build directory does not exist. Run 'make build-coverage' first."; \
	fi

.PHONY: coverage-show
coverage-show: build-coverage postgres-up
	cmake --build $(BUILD_DIR) --target coverage-show

.PHONY: coverage-open
coverage-open: coverage
	@if [ -f "$(BUILD_DIR)/coverage/html/index.html" ]; then \
		if command -v open >/dev/null 2>&1; then \
			open $(BUILD_DIR)/coverage/html/index.html; \
		elif command -v xdg-open >/dev/null 2>&1; then \
			xdg-open $(BUILD_DIR)/coverage/html/index.html; \
		else \
			echo "Coverage report generated at: $(BUILD_DIR)/coverage/html/index.html"; \
			echo "Please open this file in your web browser."; \
		fi; \
	else \
		echo "Coverage report not found. Run 'make coverage' first."; \
	fi

# Documentation
.PHONY: docs
docs:
	@echo "Generating documentation with Doxygen..."
	@if command -v doxygen >/dev/null 2>&1; then \
		doxygen Doxyfile; \
		echo "Documentation generated at docs/api/html/index.html"; \
	else \
		echo "Error: Doxygen not found. Please install Doxygen to generate documentation."; \
		echo "On macOS: brew install doxygen"; \
		echo "On Ubuntu/Debian: sudo apt-get install doxygen"; \
		exit 1; \
	fi

.PHONY: docs-open
docs-open: docs
	@if [ -f "docs/api/html/index.html" ]; then \
		if command -v open >/dev/null 2>&1; then \
			open docs/api/html/index.html; \
		elif command -v xdg-open >/dev/null 2>&1; then \
			xdg-open docs/api/html/index.html; \
		else \
			echo "Documentation generated at: docs/api/html/index.html"; \
			echo "Please open this file in your web browser."; \
		fi; \
	else \
		echo "Documentation not found. Run 'make docs' first."; \
	fi

.PHONY: docs-clean
docs-clean:
	rm -rf docs/api

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
	find src include -name "*.cpp" -o -name "*.hpp" | \
		xargs -n1 -P$$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) -I{} \
		clang-tidy -p $(BUILD_DIR) --header-filter='.*/(include|test)/.*\.(h|hpp)$$' {}

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
	@echo "  build            - Build the project using system dependencies (default)"
	@echo "  build-release    - Build the project in release mode"
	@echo "  build-coverage   - Build the project with code coverage instrumentation"
	@echo ""
	@echo "Build and test:"
	@echo "  clean            - Remove build directory"
	@echo "  test             - Build and run the tests with CTest (starts PostgreSQL)"
	@echo ""
	@echo "Code coverage:"
	@echo "  coverage         - Generate code coverage report"
	@echo "  coverage-clean   - Clean coverage data files"
	@echo "  coverage-show    - Generate coverage report and display summary"
	@echo "  coverage-open    - Generate coverage report and open it in browser"
	@echo ""
	@echo "Documentation:"
	@echo "  docs             - Generate API documentation with Doxygen"
	@echo "  docs-open        - Generate API documentation and open it in browser"  
	@echo "  docs-clean       - Remove generated documentation"
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