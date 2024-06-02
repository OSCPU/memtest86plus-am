NAME = memtest86+
SRCS = $(shell find app tests lib system -name "*.c") boot/startup32.S
CFLAGS += -Iboot -Isystem -Ilib -Itests -Iapp
include $(AM_HOME)/Makefile
