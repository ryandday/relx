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

.PHONY: test
test: postgres-up
	cd $(BUILD_DIR) && cmake --build . --target sqllib_tests --parallel 4 && ./sqllib_tests --gtest_color=yes

# Development helpers
.PHONY: format
format:
	find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Docker Compose commands
.PHONY: postgres-up
postgres-up:
	docker-compose up -d postgres
	# Wait for PostgreSQL to be ready
	docker-compose exec -T postgres sh -c "until pg_isready -U postgres; do sleep 1; done"

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
	@echo "  postgres-up      - Start PostgreSQL container"
	@echo "  postgres-down    - Stop PostgreSQL container"
	@echo "  postgres-logs    - Show PostgreSQL container logs"
	@echo "  postgres-clean   - Remove PostgreSQL container and volumes"