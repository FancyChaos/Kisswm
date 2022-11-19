PREFIX = /usr/local

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

XRANDRLIBS  = -lXrandr

FREETYPELIBS = -lfontconfig -lXft
FREETYPEINC = ${X11INC}/freetype2
MANPREFIX = ${PREFIX}/man

INCS = -I$(X11INC) -I$(FREETYPEINC)
LIBS = -L$(X11LIB) -lX11 $(XRANDRLIBS) $(FREETYPELIBS)

CFLAGS = -std=c99 -Wall -pedantic -Wconversion -O3 $(INCS)

CC = cc
