NAME = memtest86+
SRCS = $(shell find app tests lib system -name "*.c")
CFLAGS += -Iboot -Isystem -Ilib -Itests -Iapp
include $(AM_HOME)/Makefile
