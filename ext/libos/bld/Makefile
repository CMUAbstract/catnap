LIB=libos

CFILES=$(shell find ../../src -name '*.c')
SRC=$(subst ../../src/,,$(CFILES))
OBJECTS = $(SRC:.c=.o)

override SRC_ROOT = ../../src

override CFLAGS += \
	-I../../src/include \
	-I../../src/include/$(LIB)\
	-I../../../libio/src/include \
	-I../../../libmsp/src/include \
