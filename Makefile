CC=gcc
CFLAGS=-Wall -g -O2 -std=c99 -D_POSIX_C_SOURCE=199309 -D_BSD_SOURCE -lrt

FILES=sar.c post.c xattr.c html.c dir.c fsm.c decode.c post_fmt3.tab.c \
	post_fmt3.lex.c listing.c \
	main.c archive.c category.c comment.c feed.c index.c story.c
BINS=story index archive category feed comment

all: $(BINS)

clean:
	rm -f $(BINS) post_fmt3.lex.c post_fmt3.tab.c post_fmt3.tab.h

tags:
	cscope -R -b

index: $(FILES)
	$(CC) $(CFLAGS) -o $@ $(FILES)

story: index
	ln -fs $< $@

archive: index
	ln -fs $< $@

category: index
	ln -fs $< $@

feed: index
	ln -fs $< $@

comment: index
	ln -fs $< $@

%.lex.c: %.l %.tab.h
	lex -o $@ $<

%.tab.c %.tab.h: %.y
	bison -d $<
