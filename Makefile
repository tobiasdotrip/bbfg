CC := clang
PKG_CONFIG := pkg-config

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
	-Wvla \
	-Werror=implicit-function-declaration \
	-Werror=implicit-int

DEBUGFLAGS ?= -g
OPTFLAGS ?= -O0
CFLAGS := -std=c99 -pedantic $(WARNINGS) $(DEBUGFLAGS) $(OPTFLAGS) -MMD -MP
LIBGIT2_CFLAGS := $(shell $(PKG_CONFIG) --cflags libgit2)
CPPFLAGS := $(patsubst -I%,-isystem %,$(LIBGIT2_CFLAGS))
LDLIBS := $(shell $(PKG_CONFIG) --libs libgit2)

.PHONY: all clean compdb format tidy

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

format:
	clang-format -i src/*.[ch]

tidy:
	clang-tidy $(SRC) -- $(CPPFLAGS) $(CFLAGS)

clean:
	rm -rf $(BUILD_DIR)

compdb:
	bear -- make clean all

-include $(DEP)
