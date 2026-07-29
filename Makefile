CC = cc
CFLAGS = -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I/usr/local/include
LDFLAGS = -L/usr/local/lib -lX11

SRC = src/mriya.c src/parser.c
OBJ = mriya.o parser.o
TARGET = mriya

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

mriya.o: src/mriya.c
	$(CC) $(CFLAGS) -c src/mriya.c -o mriya.o

parser.o: src/parser.c src/parser.h
	$(CC) $(CFLAGS) -c src/parser.c -o parser.o

clean:
	rm -f $(OBJ) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/
