

all:
	gcc library.c psd_hashmap.c `curl-config --libs` -lmd -g

