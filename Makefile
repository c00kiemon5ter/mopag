# Makefile for monsterwm - see LICENSE for license and copyright information

VERSION = matus-mail
APPNAME = mopag

PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/include/X11
X11LIB = /usr/lib/X11

INCS = -I. -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11

CPPFLAGS = -DVERSION=\"${VERSION}\" -DAPPNAME=\"${APPNAME}\"
CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Os ${INCS} ${CPPFLAGS}
LDFLAGS  = -s ${LIBS}

CC 	 = cc
EXEC = ${APPNAME}

SRC = ${APPNAME}.c
OBJ = ${SRC:.c=.o}

all: options ${APPNAME}

options:
	@echo ${APPNAME} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${APPNAME}: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -fv ${APPNAME} ${OBJ} ${APPNAME}-${VERSION}.tar.gz

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@install -Dm755 ${APPNAME} ${DESTDIR}${PREFIX}/bin/${APPNAME}
	#@echo installing manual page to ${DESTDIR}${MANPREFIX}/man.1
	#@install -Dm644 ${APPNAME}.1 ${DESTDIR}${MANPREFIX}/man1/${APPNAME}.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/${APPNAME}
	#@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	#@rm -f ${DESTDIR}${MANPREFIX}/man1/${APPNAME}.1

.PHONY: all options clean install uninstall
