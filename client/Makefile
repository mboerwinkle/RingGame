.PHONY: clean
.DEFAULT: ringgameclient
CC = gcc
CFLAGS = -O0 -g -Wall
LFLAGS = -lm -lGL -lGLU -lglfw -lpthread -lepoxy
ringgameclient: *.c *.h
	$(CC) $(CFLAGS) -o ringgameclient *.c $(LFLAGS)
clean:
	rm -f ringgameclient
