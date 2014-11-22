VERSION = 1.0
PREFIX = /usr/local

INCS = -I. -I/usr/include
LIBS = -L/usr/lib -lc

CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS = -fstack-protector-all -fbounds-check -std=c99 -pedantic -Wall -Wextra ${INCS} ${CPPFLAGS} -g
LDFLAGS = ${LIBS}
CC = cc

SRC = ircl.c
OBJ = ${SRC:.c=.o}

all: ircl

.c.o:
	${CC} -c ${CFLAGS} $<

ircl: ${OBJ}
	${CC} -o $@ ${LDFLAGS} -lreadline -ltermcap ${OBJ} 

clean:
	@echo cleaning
	@rm -f ircl ${OBJ} *.core
