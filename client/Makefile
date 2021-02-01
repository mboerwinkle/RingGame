.PHONY: clean
.DEFAULT: ringgameclient
CC = gcc
CFLAGS = -Wall
LFLAGS = -lm -lGL -lGLU -lglfw -lpthread -lepoxy

ifneq ($(BUILD), debug)
	BUILD = release
endif

ifeq ($(BUILD), debug)
	CFLAGS += -O0 -g
else
	CFLAGS += -O3 -DNDEBUG
endif


ringgameclient: *.c *.h
	$(CC) $(CFLAGS) -o ringgameclient *.c $(LFLAGS)
clean:
	rm -f ringgameclient
