C_FLAGS=-std=c99 -Wall -Wextra -Werror -lm -g
CC=gcc

all: cluster.c
	${CC} ${C_FLAGS} cluster.c -o cluster

clean:
	rm -rf cluster
	rm -rf *.o
