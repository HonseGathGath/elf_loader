CC = gcc
CFLAGS = -Wall -Wextra -g -ggdb
TARGET_CFLAGS = -static -fno-stack-protector -fno-pie -no-pie -ggdb

all: bin/loader bin/target bin/vuln_target

bin/loader: src/loader.c src/loader.h src/main.c
	$(CC) $(CFLAGS) src/loader.c src/main.c -o bin/loader

bin/target: targets/target.c
	$(CC) $(TARGET_CFLAGS) targets/target.c -o bin/target

bin/vuln_target: targets/vuln_target.c
	$(CC) $(TARGET_CFLAGS) targets/vuln_target.c -o bin/vuln_target

clean:
	rm -rf bin/*
