
all:
	gcc main.c machine.c hashmap.c parser.c -lmd -g -Wall -Wextra -pedantic -Wno-unused-label

