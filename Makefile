CC := clang
CFLAGS := -Wall -Wextra -pedantic
AR := llvm-ar

all: sqlite3.lib terminder.exe

terminder.exe: src/terminder.c
	$(CC) $(CFLAGS) -o $@ $^ -lsqlite3

sqlite3.lib: sqlite3.o shell.o
	$(AR) -rcs $@ $^

shell.o: src/sqlite3/shell.c
	$(CC) $(CFLAGS) -O3 -I src/sqlite3 -c -o $@ $^

sqlite3.o: src/sqlite3/sqlite3.c
	$(CC) $(CFLAGS) -O3 -I src/sqlite3 -c -o $@ $^