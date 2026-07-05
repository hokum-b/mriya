CC = cc
CFLAGS = -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L -I/usr/local/include
LDFLAGS = -L/usr/local/lib -lX11

SRC = src/mriya.c
OBJ = mriya.o
TARGET = mriya

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)

mriya.o: src/mriya.c src/config.h
	$(CC) $(CFLAGS) -c src/mriya.c -o mriya.o

clean:
	rm -f $(OBJ) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/
