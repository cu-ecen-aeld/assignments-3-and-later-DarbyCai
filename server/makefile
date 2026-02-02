TARGET ?=aesdsocket
CC ?=$(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?= -lpthread -lrt

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS) 
		
clean: 
	rm -f aesdsocket *.o


