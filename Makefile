CC=gcc
CFLAGS=-Wall -g -O2 -std=c99 -D_POSIX_C_SOURCE=199309 -lrt

FILES=sar.c post.c xattr.c html.c

all: story

story: story.c $(FILES)
	$(CC) $(CFLAGS) -o $@ story.c $(FILES)
