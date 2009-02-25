CC=gcc
CFLAGS=-Wall -g -O2 -std=c99 -D_POSIX_C_SOURCE=199309 -lrt

FILES=sar.c post.c xattr.c html.c dir.c

all: story index archive category feed comment

clean:
	rm -f story index

index: index.c $(FILES)
	$(CC) $(CFLAGS) -o $@ index.c $(FILES)

story: story.c $(FILES)
	$(CC) $(CFLAGS) -o $@ story.c $(FILES)

archive: archive.c $(FILES)
	$(CC) $(CFLAGS) -o $@ archive.c $(FILES)

category: category.c $(FILES)
	$(CC) $(CFLAGS) -o $@ category.c $(FILES)

feed: feed.c $(FILES)
	$(CC) $(CFLAGS) -o $@ feed.c $(FILES)

comment: comment.c $(FILES)
	$(CC) $(CFLAGS) -o $@ comment.c $(FILES)
