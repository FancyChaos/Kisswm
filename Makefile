include config.mk

all: kisswm

kisswm: kisswm.o util.o
	$(CC) kisswm.o util.o -o kisswm $(LIBS)

kisswm.o: kisswm.c kisswm.h layouts.c
	$(CC) -c kisswm.c $(CFLAGS)

util.o: util.c util.h
	$(CC) -c util.c $(CFLAGS)

install: kisswm
	rm $(DESTDIR)$(PREFIX)/bin/kisswm || true
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp kisswm $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/kisswm

uninstall:
	rm -rf $(DESTDIR)$(PREFIX)/bin/kisswm

clean:
	rm -f *.o kisswm *.core

.PHONY: all install uninstall clean
