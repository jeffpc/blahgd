#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "dir.h"
#include "post.h"

#define LIST_BLOCK_SIZE		128

static int cmp_asc(const void *a, const void *b)
{
	char *as = *(char**)a, *bs = *(char**)b;

	return strcmp(as, bs);
}

static int cmp_desc(const void *a, const void *b)
{
	char *as = *(char**)a, *bs = *(char**)b;

	return -strcmp(as, bs);
}

int sorted_readdir_loop(DIR *dir, struct post *post,
			void(*f)(struct post*, char*, void*), void *data,
			int updown, int limit)
{
	struct dirent *de;
	char **list = NULL;
	int size = 0;
	int count = 0;
	int i;

	while((de = readdir(dir))) {
		if (!strcmp(de->d_name, ".") ||
		    !strcmp(de->d_name, ".."))
			continue;

		if (!list || count == size) {
			size += LIST_BLOCK_SIZE;
			list = realloc(list, sizeof(char*)*size);
			if (!list) {
				printf("%s: couldn't realloc\n", __func__);
				return ENOMEM;
			}
		}

		list[count] = strdup(de->d_name);
		count++;
	}

	if (updown == SORT_ASC)
		qsort(list, count, sizeof(char*), cmp_asc);
	else
		qsort(list, count, sizeof(char*), cmp_desc);

	for(i=0; (i<count) && limit; i++) {
		f(post, list[i], data);
		free(list[i]);

		if (limit != -1)
			limit--;
	}

	return 0;
}
