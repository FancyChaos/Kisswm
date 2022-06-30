include config.mk

all: kisswm

kisswm: kisswm.o
	$(CC) kisswm.o -o kisswm $(LIBS)

kisswm.o: kisswm.c kisswm.h
	$(CC) -c kisswm.c $(CFLAGS)

install: kisswm
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp kisswm $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/kisswm

uninstall:
	rm -rf $(DESTDIR)$(PREFIX)/bin/kisswm

clean:
	rm -f *.o kisswm

.PHONY: all install uninstall clean
