CC_FLAGS = -Wall -Wextra -Wpedantic -g -fsanitize=address -fno-omit-frame-pointer

SRC =  src/sighandlers.c src/syscall_wrappers.c src/parse.c src/builtins.c src/redirect.c src/shell.c src/main.c
OBJ = $(SRC:.c=.o)

TARGET = seashell

all: $(TARGET)

$(TARGET): $(OBJ)
	gcc $(OBJ) -o $(TARGET) $(CC_FLAGS)

%.o: %.c
	gcc -c $< -o $@ $(CC_FLAGS)

clean:
	rm -f $(OBJ) $(TARGET)
