CC=gcc
CFLAGS=-Wall -g -O2

FILES=sar.c post.c xattr.c html.c

all: story

story: story.c $(FILES)
	$(CC) $(CFLAGS) -o $@ story.c $(FILES)
