UNAME := $(shell uname)

PREFIX = /usr/local

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

XRANDRLIBS  = -lXrandr

FREETYPELIBS = -lfontconfig -lXft

ifeq ($(UNAME), Linux)
MANPREFIX = ${PREFIX}/share/man
FREETYPEINC = /usr/include/freetype2
LIBBSD = -lbsd
LIBS_OS = $(LIBBSD)
endif
ifeq ($(UNAME), OpenBSD)
MANPREFIX = ${PREFIX}/man
FREETYPEINC = ${X11INC}/freetype2
LIBS_OS =
endif

INCS = -I$(X11INC) -I$(FREETYPEINC)
LIBS = -L$(X11LIB) -lX11 $(LIBS_OS) $(XRANDRLIBS) $(FREETYPELIBS)

CFLAGS = -MMD -MP -std=c99 -Wall -pedantic -Wconversion -O2 $(INCS)
