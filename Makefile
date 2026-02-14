CC := clang
CFLAGS := -Wall -Wextra -pedantic -D_CRT_SECURE_NO_WARNINGS -g -fsanitize=address

build/terminder.exe: terminder.c
	$(CC) $(CFLAGS) -o $@ $^
