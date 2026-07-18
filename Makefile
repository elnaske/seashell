CC_FLAGS = -Wall -Wextra -Wpedantic -g -fsanitize=address -fno-omit-frame-pointer

SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=build/%.o)

TARGET = seashell

all: $(TARGET)

$(TARGET): $(OBJ)
	gcc $(OBJ) -o $(TARGET) $(CC_FLAGS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	gcc -c $< -o $@ $(CC_FLAGS)

clean:
	rm -f $(OBJ) $(TARGET)
