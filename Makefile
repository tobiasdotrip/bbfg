CC := clang
PKG_CONFIG := pkg-config
export PKG_CONFIG_PATH := /usr/local/lib/pkgconfig$(if $(PKG_CONFIG_PATH),:$(PKG_CONFIG_PATH))

TARGET := bbfg
BUILD_DIR := build
SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=$(BUILD_DIR)/%.o)
DEP := $(OBJ:.o=.d)

WARNINGS := \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wshadow \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Wold-style-definition \
	-Wnull-dereference \
	-Wvla \
	-Werror=implicit-function-declaration \
	-Werror=implicit-int

DEBUGFLAGS ?= -g
OPTFLAGS ?= -O0
SANITIZER_FLAGS ?=
LDFLAGS ?=
LIBGIT2_CFLAGS := $(shell $(PKG_CONFIG) --cflags libgit2)
LIBGIT2_HAS_THIN_PACK := $(shell $(PKG_CONFIG) --atleast-version=1.9.0 libgit2 && echo 1 || echo 0)
CFLAGS := -DBBFG_LIBGIT2_HAS_THIN_PACK=$(LIBGIT2_HAS_THIN_PACK) -D_POSIX_C_SOURCE=200809L -std=c99 -pedantic $(WARNINGS) $(DEBUGFLAGS) $(OPTFLAGS) $(SANITIZER_FLAGS) -MMD -MP
CPPFLAGS := $(patsubst -I%,-isystem %,$(LIBGIT2_CFLAGS))
LDLIBS := $(shell $(PKG_CONFIG) --libs libgit2)
TEST_SRC := $(wildcard tests/*.c)
TEST_OBJ := $(TEST_SRC:tests/%.c=$(BUILD_DIR)/tests/%.o)
TEST_DEP := $(TEST_OBJ:.o=.d)
TEST_TARGET := $(BUILD_DIR)/$(TARGET)-tests
CRITERION_CFLAGS := $(shell $(PKG_CONFIG) --cflags criterion)
CRITERION_CPPFLAGS := $(patsubst -I%,-isystem %,$(CRITERION_CFLAGS))
CRITERION_LDLIBS := $(shell $(PKG_CONFIG) --libs criterion)
TEST_ARGS ?=

.PHONY: all benchmark clean compdb format release test test-sanitize tidy

all: $(BUILD_DIR)/$(TARGET)

release:
	$(MAKE) clean
	$(MAKE) DEBUGFLAGS= OPTFLAGS=-O3 all

benchmark: release
	./bench/rewrite.sh

$(BUILD_DIR)/$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: tests/%.c
	@mkdir -p $(BUILD_DIR)/tests
	$(CC) $(CPPFLAGS) $(CRITERION_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_OBJ)
	$(CC) $(LDFLAGS) $(TEST_OBJ) $(CRITERION_LDLIBS) -o $@

format:
	clang-format -i src/*.[ch] tests/*.[ch]

test: all $(TEST_TARGET)
	BBFG=./$(BUILD_DIR)/$(TARGET) $(TEST_TARGET) $(TEST_ARGS)

test-sanitize:
	$(MAKE) clean
	$(MAKE) SANITIZER_FLAGS='-fsanitize=address,undefined' \
		LDFLAGS='-fsanitize=address,undefined' DEBUGFLAGS=-g OPTFLAGS=-O1 test

tidy:
	clang-tidy --quiet $(SRC) $(TEST_SRC) -- $(CPPFLAGS) $(CRITERION_CPPFLAGS) $(CFLAGS)

clean:
	rm -rf $(BUILD_DIR)

compdb:
	bear -- make clean all

-include $(DEP) $(TEST_DEP)
