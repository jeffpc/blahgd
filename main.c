#include <stdio.h>
#include <string.h>
#include <libgen.h>

extern int blahg_archive(int argc, char **argv);
extern int blahg_category(int argc, char **argv);
extern int blahg_comment(int argc, char **argv);
extern int blahg_feed(int argc, char **argv);
extern int blahg_index(int argc, char **argv);
extern int blahg_story(int argc, char **argv);

int main(int argc, char **argv)
{
	char *bn;

	bn = basename(argv[0]);

	if (!strcmp(bn, "archive"))
		return blahg_archive(argc, argv);
	if (!strcmp(bn, "category"))
		return blahg_category(argc, argv);
	if (!strcmp(bn, "comment"))
		return blahg_comment(argc, argv);
	if (!strcmp(bn, "feed"))
		return blahg_feed(argc, argv);
	if (!strcmp(bn, "index"))
		return blahg_index(argc, argv);
	if (!strcmp(bn, "story"))
		return blahg_story(argc, argv);

	printf("Content-Type: text/plain\n\nInvalid filename.");
	return 0;
}
