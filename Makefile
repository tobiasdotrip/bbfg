CC := clang

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
	-Wdeclaration-after-statement \
	-Wvla \
	-Werror=implicit-function-declaration \
	-Werror=implicit-int

CFLAGS := -std=c99 -pedantic -g -O0 $(WARNINGS) -MMD -MP

.PHONY: all clean format tidy

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

format:
	clang-format -i src/*.[ch]

tidy:
	clang-tidy $(SRC) -- $(CFLAGS)

clean:
	rm -rf $(BUILD_DIR)

-include $(DEP)
