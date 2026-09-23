CC = gcc
CFLAGS = -std=gnu99 -Wall

TARGET = pocketsh

all: $(TARGET)

$(TARGET): pocketsh.c pocketsh.h
	$(CC) $(CFLAGS) -o $(TARGET) pocketsh.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
