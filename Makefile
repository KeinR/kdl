

all:
	gcc library.c `curl-config --libs` -g

