CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET_CFLAGS = -static -fno-stack-protector

all: bin/loader bin/target

bin/loader: src/loader.c
	$(CC) $(CFLAGS) src/loader.c -o bin/loader

bin/target: targets/target.c
	$(CC) $(TARGET_CFLAGS) targets/target.c -o bin/target

clean:
	rm -rf bin/*
