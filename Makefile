VERSION = 1.0
PREFIX = /usr/local

INCS_ALL = -I. -I/usr/include
LIBS_ALL = -L/usr/lib -lc

INCS = ${INCS_ALL}
LIBS = ${LIBS_ALL}
## Uncomment for OSX (after running `brew install readline`)
#INCS = -I/usr/local/opt/readline/include ${INCS_ALL}
#LIBS = -L/usr/local/Cellar/readline/6.3.8/lib ${LIBS_ALL}



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
