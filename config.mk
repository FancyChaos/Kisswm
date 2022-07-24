PREFIX = /usr/local

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

XINERAMALIBS  = -lXinerama

# freetype
FREETYPELIBS = -lfontconfig -lXft
FREETYPEINC = /usr/include/freetype2

# OpenBSD (uncomment)
FREETYPEINC = ${X11INC}/freetype2
MANPREFIX = ${PREFIX}/man

INCS = -I$(X11INC) -I$(FREETYPEINC)
LIBS = -L$(X11LIB) -lX11 $(XINERAMALIBS) $(FREETYPELIBS)

#CFLAGS = -std=c99 -Wall -pedantic -Wconversion -O $(INCS)
CFLAGS = -std=c99 -Wall -pedantic -O3 $(INCS)

CC = cc
