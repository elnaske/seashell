CC_FLAGS = -Wall -Wextra -Wpedantic -g -fsanitize=address -fno-omit-frame-pointer

SRC = src/main.c src/sighandlers.c src/syscalls.c
OBJ = $(SRC:.c=.o)

TARGET = seashell

all: $(TARGET)

$(TARGET): $(OBJ)
	gcc $(OBJ) -o $(TARGET) $(CC_FLAGS)

%.o: %.c
	gcc -c $< -o $@ $(CC_FLAGS)

clean:
	rm -f $(OBJ) $(TARGET)
