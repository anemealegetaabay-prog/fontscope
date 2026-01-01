CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -O2
BUILD    := build
LIB      := $(BUILD)/libfontscope.a

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(SRCS))

TEST_SRCS := $(wildcard tests/test_*.cpp)
TEST_BINS := $(patsubst tests/%.cpp,$(BUILD)/%,$(TEST_SRCS))

CLI_SRC  := tools/fontscope.cpp
CLI_BIN  := $(BUILD)/fontscope

.PHONY: all lib cli tests test sanitize clean

all: lib cli tests

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB): $(OBJS) | $(BUILD)
	ar rcs $@ $^

lib: $(LIB)

$(CLI_BIN): $(CLI_SRC) $(LIB) | $(BUILD)
	$(CXX) $(CXXFLAGS) $< $(LIB) -o $@

cli: $(CLI_BIN)

$(BUILD)/test_%: tests/test_%.cpp $(LIB) | $(BUILD)
	$(CXX) $(CXXFLAGS) $< $(LIB) -o $@

tests: $(TEST_BINS)

test: tests
	@for t in $(TEST_BINS); do echo "Running $$t..."; $$t || exit 1; done
	@echo "All tests passed."

sanitize: CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g
sanitize: CXX = clang++
sanitize: clean all

clean:
	rm -rf $(BUILD)
