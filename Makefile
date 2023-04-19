

all:
	gcc library.c psd_hashmap.c `curl-config --libs` -lmd -lc -g -Wall -Wextra -Wno-unused-parameter

