CC ?= gcc
RM = rm -f

CCFLAGS = -Wall -D_GNU_SOURCE
ifeq ($(DEBUG),1)
	CCFLAGS += -O1 -ggdb
else
	CCFLAGS += -Ofast 
endif

DEPS = libosmocore
DEPFLAGS_CC = `pkg-config --cflags $(DEPS)`
DEPFLAGS_LD = `pkg-config --libs $(DEPS)`
OBJS = $(patsubst %.c,%.o,$(wildcard *.c))
HDRS = $(wildcard *.h)

all: ../src/x75

%.o : %.c $(HDRS) Makefile
	$(CC) -c $(CCFLAGS) $(DEPFLAGS_CC) $< -o $@

x75: $(OBJS)
	$(CC) $(CCFLAGS) $^ $(DEPFLAGS_LD) -o x75

pppdtest: $(OBJS)
	$(CC) $(CCFLAGS) $^ $(DEPFLAGS_LD) -o pppdtest

clean:
	$(RM) $(OBJS)
	$(RM) x75

.PHONY: all clean