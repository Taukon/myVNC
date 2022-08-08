CC = gcc
CFLAGS = -Wall

ALL: vnc

vnc: jisakuvnc.c
		$(CC) $(CFLAGS) -o jisakuvnc jisakuvnc.c -lvncserver -lX11 -lXtst
		cp jisakuvnc ../myvnc

clean:
		rm -f jisakuvnc ../myvnc