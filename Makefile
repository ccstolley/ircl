VERSION = 1.0
PREFIX = /usr/local

INCS_ALL = -I/usr/include
LIBS_ALL = -L/usr/lib -lc -lssl -lcrypto -lncurses -lreadline

INCS = ${INCS_ALL}
LIBS = ${LIBS_ALL}
SRC = ircl.c

CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS = -fstack-protector-all -fbounds-check -std=gnu11 -pedantic -Wall -Wextra ${INCS} ${CPPFLAGS} -g
LDFLAGS = ${LIBS}
CC = cc

OBJ = ${SRC:.c=.o}

all: ircl

.c.o:
	${CC} -c ${CFLAGS} $<

ircl: ${OBJ}
	${CC} -o $@ ${LDFLAGS} ${OBJ} 

clean:
	@echo cleaning
	@rm -f ircl ${OBJ} *.core *.o
