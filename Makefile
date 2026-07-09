CC_FLAGS = -Wall -Wextra -Wpedantic -g -fsanitize=address -fno-omit-frame-pointer

main: src/main.c src/sighandlers.c
	gcc src/main.c src/sighandlers.c -o seashell $(CC_FLAGS)