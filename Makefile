NAME = memtest86+
SRCS = $(shell find app tests lib system -name "*.c")
CFLAGS += -Isystem -Ilib -Itests -Iapp
include $(AM_HOME)/Makefile
