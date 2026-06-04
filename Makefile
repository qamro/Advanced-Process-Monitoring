CC     = gcc
CFLAGS = -O2 -Wall -Wextra -Wno-unused-result
LIBS   = -lncurses -lm

SRC = main.c cpu.c mem.c net.c disk.c system.c ui.c

all: ghost

ghost: $(SRC)
	$(CC) $(SRC) -o ghost $(CFLAGS) $(LIBS)

clean:
	rm -f ghost
