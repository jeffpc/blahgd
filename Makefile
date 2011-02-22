CC=gcc
CFLAGS=-Wall -g -O2 -std=c99 -D_POSIX_C_SOURCE=199309 -D_BSD_SOURCE -lrt

FILES=sar.c post.c xattr.c html.c dir.c fsm.c decode.c post_fmt3.tab.c \
	post_fmt3.lex.c listing.c \
	main.c archive.c category.c comment.c feed.c index.c story.c
BINS=blahg

all: $(BINS)

clean:
	rm -f $(BINS) post_fmt3.lex.c post_fmt3.tab.c post_fmt3.tab.h

tags:
	cscope -R -b

blahg: $(FILES)
	$(CC) $(CFLAGS) -o $@ $(FILES)

%.lex.c: %.l %.tab.h
	lex -o $@ $<

%.tab.c %.tab.h: %.y
	bison -d $<
