#!/usr/bin/env make
PROJECT= prog
CC= gcc
SRC= prog.c
CWARN= -Wall -Wextra -Wno-misleading-indentation

CSTD= -std=c99

BITS := $(shell uname -p)
ifeq ($(BITS), x86_64)
    ARCH= -m64
    # write
    # ptrace
    # exit    
    CDEFINE= -DP=1 -DT=101 -DE=60
else
    ARCH= -m32
    # write
    # ptrace
    # exit    
    CDEFINE= -DP=4 -DT=26 -DE=1
endif

OPT= -g

CFLAGS= ${CWARN} ${CSTD} ${ARCH} ${CDEFINE} ${OPT}
LDFLAGS= -ldl

RM= rm

all:  ${PROJECT}

${PROJECT}:
	${CC} ${CFLAGS} ${SRC} -o $@ ${LDFLAGS}
clean:
	${RM} -f ${PROJECT}
