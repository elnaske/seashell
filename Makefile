CC_FLAGS = -Wall -Wextra -Wpedantic -g -fsanitize=address -fno-omit-frame-pointer

SRC := $(shell find src -name '*.c')
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))

TARGET = seashell

all: $(TARGET)

$(TARGET): $(OBJ)
	gcc $(OBJ) -o $@ $(CC_FLAGS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	gcc -c $< -o $@ $(CC_FLAGS)

clean:
	rm -rf build $(TARGET)