CC_FLAGS = -Wall -Wextra -Wpedantic -g -fsanitize=address -fno-omit-frame-pointer
LIBS = -lreadline

SRC := $(shell find src -name '*.c')
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))

TARGET = seashell

all: $(TARGET)

$(TARGET): $(OBJ)
	gcc $(OBJ) -o $@ $(CC_FLAGS) $(LIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	gcc -c $< -o $@ $(CC_FLAGS) $(LIBS)

clean:
	rm -rf build $(TARGET)