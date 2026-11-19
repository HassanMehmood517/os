# Build every .c file in this folder into a program with the same name.
# Usage:  make        (build all)
#         make clean  (remove the programs)
CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread
SRCS    = $(wildcard *.c)
PROGS   = $(SRCS:.c=)

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)

.PHONY: all clean
