CC := clang
CFLAGS := -Wall -Wextra -pedantic
AR := llvm-ar


ifeq ($(OS), Windows_NT)
	LIBSQLITE  := sqlite3.lib
	EXECUTABLE := terminder.exe
else
	LIBSQLITE  := libsqlite3.a
	EXECUTABLE := terminder
endif

all: sqlite3.lib terminder.exe

$(EXECUTABLE): src/terminder.c
	$(CC) $(CFLAGS) -o $@ $^ -lsqlite3

$(LIBSQLITE): sqlite3.o shell.o
	$(AR) -rcs $@ $^

shell.o: src/sqlite3/shell.c
	$(CC) $(CFLAGS) -O3 -I src/sqlite3 -c -o $@ $^

sqlite3.o: src/sqlite3/sqlite3.c
	$(CC) $(CFLAGS) -O3 -I src/sqlite3 -c -o $@ $^
