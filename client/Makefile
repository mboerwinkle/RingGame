.PHONY: clean
.DEFAULT: ringgameclient
CC = gcc
CFLAGS = -Wall
LFLAGS = -lm -lpthread

ifneq ($(BUILD), debug)
	BUILD = release
endif

ifeq ($(BUILD), debug)
	CFLAGS += -O0 -g
else
	CFLAGS += -O3 -DNDEBUG
endif

PKG_LIBS = gl glu glfw3 epoxy libutf8proc
CFLAGS += `pkg-config --cflags $(PKG_LIBS)`
LFLAGS += `pkg-config --libs $(PKG_LIBS)`

ringgameclient: *.c *.h
	$(CC) $(CFLAGS) -o ringgameclient *.c $(LFLAGS)
clean:
	rm -f ringgameclient
