C_FLAGS=-Wall -Werror -Wextra -std=c99 -g -lm
CC=gcc

all: t9search.c
	${CC} ${C_FLAGS} t9search.c -o t9search

bitap: bitap.c
	${CC} ${C_FLAGS} bitap.c -o bitap

clean:
	rm -rf t9search
	rm -rf bitap
	rm -rf *.o
