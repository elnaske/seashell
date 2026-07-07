CC_FLAGS = -Wall -Wextra -Wpedantic -g -fsanitize=address -fno-omit-frame-pointer

main: src/main.c
	gcc src/main.c -o seashell $(CC_FLAGS)