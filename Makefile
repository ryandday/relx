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
	@echo "  format         - Format code using clang-format"