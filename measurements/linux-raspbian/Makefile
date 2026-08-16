CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Wno-unused-variable -std=c11
TARGET  = ground_station

all: $(TARGET)

$(TARGET): ground_station.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: all clean install
