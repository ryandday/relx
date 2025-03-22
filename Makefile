# Project configuration
BUILD_DIR := build
BUILD_TYPE ?= Debug

# Conan commands
.PHONY: conan-install
conan-install:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && conan install .. --build=missing

# Build commands
.PHONY: configure
configure: conan-install
	cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

.PHONY: build
build: configure
	cd $(BUILD_DIR) && cmake --build .

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# Run the executable
.PHONY: run
run: build
	./$(BUILD_DIR)/schema_example

# Test commands
.PHONY: test
test: build
	cd $(BUILD_DIR) && cmake --build . --target sqllib_tests --parallel && ctest --output-on-failure -V

.PHONY: test-verbose
test-verbose: build
	cd $(BUILD_DIR) && cmake --build . --target sqllib_tests --parallel && ./sqllib_tests --gtest_color=yes

.PHONY: static-test
static-test: configure
	cd $(BUILD_DIR) && cmake --build . --target static_assert_tests --parallel && ./static_assert_tests

# Development helpers
.PHONY: format
format:
	find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  conan-install  - Install dependencies using Conan"
	@echo "  configure      - Configure project with CMake"
	@echo "  build          - Build the project"
	@echo "  clean          - Remove build directory"
	@echo "  run            - Build and run the application"
	@echo "  test           - Build and run the tests with CTest"
	@echo "  test-verbose   - Build and run tests directly with more detailed output"
	@echo "  static-test    - Build and run the static assert tests"
	@echo "  format         - Format code using clang-format"