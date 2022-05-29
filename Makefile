LIBS =
INC = -I./include
FLAGS = -Wall -O3
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c,obj/%.o,$(SRC))

CC = gcc
BIN = bench

all:obj $(BIN)

obj:
	@mkdir -p $@

bench:bench.o btree.o
	$(CC) -o $@ $^ $(LIBS)

$(OBJ):obj/%.o:src/%.c
	$(CC) -c $(FLAGS) -o $@ $< $(INC)

clean:
	-rm $(BIN) $(OBJ)
	@rmdir obj

.PHONY:all clean

vpath %.c src
vpath %.o obj
vpath %.h include
