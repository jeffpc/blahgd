CC=gcc
CFLAGS=-Wall -g -O2 -std=c99 -D_POSIX_C_SOURCE=199309 -D_BSD_SOURCE -lrt

FILES=sar.c post.c xattr.c html.c dir.c fsm.c decode.c post_fmt3.c
BINS=story index archive category feed comment

all: $(BINS)

clean:
	rm -f $(BINS) post_fmt3.c

tags:
	cscope -R -b

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

%.c: %.l
	lex -o $@ $<
