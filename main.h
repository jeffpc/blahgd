#ifndef __MAIN_H
#define __MAIN_H

enum {
	PAGE_MALFORMED,

	PAGE_ARCHIVE,
	PAGE_CATEGORY,
	PAGE_TAG,
	PAGE_COMMENT,
	PAGE_FEED,
	PAGE_INDEX,
	PAGE_STORY,
	PAGE_XMLRPC,
};

struct qs {
	int page;

	int p;
	int paged;
	int m;
	int preview;
	int xmlrpc;
	char *cat;
	char *tag;
	char *feed;
	char *comment;
};

extern int blahg_archive(int m, int paged);
extern int blahg_category(char *cat, int paged);
extern int blahg_tag(char *tag, int paged);
extern int blahg_comment();
extern int blahg_feed(char *feed, int p);
extern int blahg_index(int paged);
extern int blahg_story(int p, int preview);
extern int blahg_pingback();

extern void disp_404(char *title, char *txt);

#endif
