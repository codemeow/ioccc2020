#!/usr/bin/env make
PROJECT=prog
CC= gcc
SRC=prog.c
CWARN=-Wall -Wextra -Wno-char-subscripts

CSTD= -std=c99
# Syscalls table
# DS - syscall nanosleep
# DO - syscall open
# DR - syscall read
# DC - syscall close

# X11 structures offsets
# dS - offset of screens in Display
# dR - offset of root in Screen
# dD - offset of root_depth in Screen
# dV - offset of root_visual in Screen
# dG - offset of default_gc in Screen

BITS := $(shell uname -p)

ifeq ($(BITS), x86_64)
    ARCH= -m64
    CDEFINE= -DDS=35 -DDO=2 -DDR=0 -DDC=3 -DdS=232 -DdR=16 -DdD=56 -DdV=64 -DdG=72
else
    ARCH= -m32
    CDEFINE= -DDS=162 -DDO=5 -DDR=3 -DDC=6 -DdS=140 -DdR=8 -DdD=36 -DdV=40 -DdG=44
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
