#ifndef __MAIN_H
#define __MAIN_H

enum {
	PAGE_MALFORMED,

	PAGE_ARCHIVE,
	PAGE_CATEGORY,
	PAGE_COMMENT,
	PAGE_FEED,
	PAGE_INDEX,
	PAGE_STORY,
};

struct qs {
	int page;

	int p;
	int paged;
	int m;
	char *cat;
	char *feed;
	char *comment;
};

extern int blahg_archive(int m, int paged);
extern int blahg_category(char *cat, int paged);
extern int blahg_comment();
extern int blahg_feed(char *feed, int p);
extern int blahg_index(int paged);
extern int blahg_story(int p);

#endif
