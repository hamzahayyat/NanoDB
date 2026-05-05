CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS  :=

BUILD_DIR := build
DATA_DIR  := data

# Source files
SRCS := src/memory/BufferPool.cpp \
        src/parser/QueryParser.cpp \
        src/main.cpp

OBJS := $(SRCS:src/%.cpp=$(BUILD_DIR)/%.o)

TARGET := $(BUILD_DIR)/test_runner

.PHONY: all clean run dirs stress

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILD_DIR)/memory $(BUILD_DIR)/parser $(DATA_DIR)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@
	@echo "Build successful: $(TARGET)"

$(BUILD_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	cd $(BUILD_DIR) && ../$(TARGET)

# Stress mode: 50-page pool for Test Case D
stress: all
	cd $(BUILD_DIR) && ../$(TARGET) --stress

clean:
	rm -rf $(BUILD_DIR) $(DATA_DIR)/*.bin nanodb_execution.log

help:
	@echo "Targets:"
	@echo "  all      - Build the test runner"
	@echo "  run      - Build and execute with full TPC-H dataset"
	@echo "  stress   - Build and run with 50-page buffer pool (Test Case D)"
	@echo "  clean    - Remove build artifacts and data files"
