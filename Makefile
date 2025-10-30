CC = gcc
CFLAGS = `pkg-config --cflags gtk4`
LIBS = `pkg-config --libs gtk4`

dy: dy.c
	$(CC) $(CFLAGS) -o dy dy.c $(LIBS)

clean:
	rm -f dy

install:
	cp dy ~/.local/bin/

.PHONY: clean install
