# Makefile for Hestia32 Thermostat Tests
# Provides convenient commands for building and running tests

.PHONY: test clean help

# Default target
help:
	@echo "Hestia32 Test Suite"
	@echo "==================="
	@echo ""
	@echo "Available targets:"
	@echo "  make test    - Build and run thermostat tests"
	@echo "  make clean   - Remove test binaries"
	@echo "  make help    - Show this help message"
	@echo ""

# Build and run thermostat tests
test:
	@echo "Building thermostat tests..."
	gcc -Wall -Wextra -std=c11 -I. \
		src/core/thermostat.c \
		tests/test_thermostat.c \
		-o test_thermostat \
		-lm
	@echo ""
	@echo "Running tests..."
	./test_thermostat

# Clean up test binaries
clean:
	rm -f test_thermostat
	@echo "Cleaned test binaries"
