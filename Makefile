NDEBUG ?= n

CC := clang
CFLAGS := -Wall -Wextra -pedantic -D_CRT_SECURE_NO_WARNINGS

ifeq ($(NDEBUG),y)
	CFLAGS += -DNDEBUG -O3
else
	CFLAGS += -g -fsanitize=address
endif

build/terminder.exe: terminder.c
	$(CC) $(CFLAGS) -o $@ $^
