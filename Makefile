CC := clang
CFLAGS := -Wall -Wextra -pedantic
LFLAGS := 
AR := llvm-ar

NDEBUG=n

ifeq ($(NDEBUG), y)
	CFLAGS += -DNDEBUG -O3
endif

ifeq ($(OS), Windows_NT)
	LIBSQLITE  := sqlite3.lib
	EXECUTABLE := terminder.exe
	CFLAGS += -D_CRT_SECURE_NO_WARNINGS
	LFLAGS += -lShlwapi
else
	LIBSQLITE  := libsqlite3.a
	EXECUTABLE := terminder
endif

all: build/$(LIBSQLITE) build/$(EXECUTABLE)

build/$(EXECUTABLE): src/terminder.c
	$(CC) $(CFLAGS) -o $@ $^ -L build -lsqlite3 $(LFLAGS)

build/$(LIBSQLITE): build/sqlite3.o build/shell.o
	$(AR) -rcs $@ $^

build/shell.o: src/sqlite3/shell.c
	$(CC) $(CFLAGS) -O3 -I src/sqlite3 -c -o $@ $^

build/sqlite3.o: src/sqlite3/sqlite3.c
	$(CC) $(CFLAGS) -O3 -I src/sqlite3 -c -o $@ $^
