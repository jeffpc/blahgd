/*
 * Copyright (c) 2015 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <stddef.h>
#include <sys/sysmacros.h>

#include "post.h"
#include "str.h"
#include "error.h"
#include "iter.h"

struct post_index_entry {
	avl_node_t node;

	/* key */
	unsigned int time;
	unsigned int id;

	/* value */
	struct post *post;
};

struct post_subindex {
	avl_node_t index;

	/* key */
	const struct str *name;

	/* value */
	avl_tree_t subindex;
};

/*
 * We need a number of different indices.  In all cases, the nodes in these
 * indices should be ordered by the publication time and ties are resolved
 * by comparing the post ids.
 *
 * The indices are:
 *
 *   - global
 *   - by time (used for index & archive listing)
 *   - by tag (used for tag listing)
 *   - by category (used for category listing)
 *
 * The global index maps a post id to a struct post pointer using struct
 * post_index_entry with the time part of the key set to zero.
 *
 * The by-time index uses struct post_index_entry to map <time, post id> to
 * a struct post.
 *
 * To maximize code reuse, the by-tag and by-category trees contain struct
 * post_subindex nodes for each (unique) tag/category.  Those nodes contain
 * AVL trees of their own with struct post_index_entry elements mapping
 * <timestamp, post id> to a struct post pointer.
 */

static avl_tree_t index_global;
static avl_tree_t index_by_time;
static avl_tree_t index_by_tag;
static avl_tree_t index_by_cat;

static pthread_mutex_t index_lock;

/*
 * Assorted comparators
 */

/* compare two post index entries by the timestamp */
static int post_index_cmp(const void *va, const void *vb)
{
	const struct post_index_entry *a = va;
	const struct post_index_entry *b = vb;

	/* first, compare by time */
	if (a->time < b->time)
		return -1;
	if (a->time > b->time)
		return 1;

	/* resolve ties by comparing post id */
	if (a->id < b->id)
		return -1;
	if (a->id > b->id)
		return 1;

	return 0;
}

/* compare two subindex entries */
static int post_tag_cmp(const void *va, const void *vb)
{
	const struct post_subindex *a = va;
	const struct post_subindex *b = vb;
	int ret;

	ret = strcasecmp(str_cstr(a->name), str_cstr(b->name));

	if (ret > 0)
		return 1;
	if (ret < 0)
		return -1;
	return 0;
}

static void init_index_tree(avl_tree_t *tree)
{
	avl_create(tree, post_index_cmp, sizeof(struct post_index_entry),
		   offsetof(struct post_index_entry, node));
}

void init_post_index(void)
{
	init_index_tree(&index_global);
	init_index_tree(&index_by_time);

	/* set up the by-tag/category indexes */
	avl_create(&index_by_tag, post_tag_cmp, sizeof(struct post_subindex),
		   offsetof(struct post_subindex, index));
	avl_create(&index_by_cat, post_tag_cmp, sizeof(struct post_subindex),
		   offsetof(struct post_subindex, index));

	MXINIT(&index_lock);
}

static avl_tree_t *__get_subindex(avl_tree_t *index, const struct str *tagname)
{
	struct post_subindex *ret;
	struct post_subindex key = {
		.name = tagname,
	};

	ret = avl_find(index, &key, NULL);
	if (!ret)
		return NULL;

	return &ret->subindex;
}

/* lookup a post based on id */
struct post *index_lookup_post(unsigned int postid)
{
	struct post_index_entry *ret;
	struct post_index_entry key = {
		.time = 0,
		.id = postid,
	};
	struct post *post;

	MXLOCK(&index_lock);
	ret = avl_find(&index_global, &key, NULL);
	if (ret)
		post = post_getref(ret->post);
	else
		post = NULL;
	MXUNLOCK(&index_lock);

	return post;
}

/* get a list of posts based on tagname & return value from predicate */
int index_get_posts(struct post **ret, const struct str *tagname, bool tag,
		    bool (*pred)(struct post *, void *), void *private,
		    int skip, int nposts)
{
	struct post_index_entry *cur;
	avl_tree_t *tree;
	int i;

	MXLOCK(&index_lock);

	if (!tagname)
		tree = &index_by_time;
	else if (tag)
		tree = __get_subindex(&index_by_tag, tagname);
	else
		tree = __get_subindex(&index_by_cat, tagname);

	ASSERT3P(tree, !=, NULL);

	/* skip over the first entries as requested */
	for (cur = avl_last(tree); cur && skip; cur = AVL_PREV(tree, cur))
		skip--;

	/* get a reference for every post we're returning */
	for (i = 0; cur && nposts; cur = AVL_PREV(tree, cur)) {
		if (pred && !pred(cur->post, private))
			continue;

		ret[i] = post_getref(cur->post);

		nposts--;
		i++;
	}

	MXUNLOCK(&index_lock);

	return i;
}

/* like avl_add, but returns the existing node */
static void *safe_avl_add(avl_tree_t *tree, void *node)
{
	avl_index_t where;
	void *tmp;

	tmp = avl_find(tree, node, &where);
	if (!tmp)
		avl_insert(tree, node, where);

	return tmp;
}

static int __insert_post_tags(avl_tree_t *index, struct post *post,
			      list_t *taglist)
{
	struct post_index_entry *tag_entry;
	struct post_subindex *sub;
	struct post_tag *tag;
	avl_index_t where;

	list_for_each(taglist, tag) {
		struct post_subindex key = {
			.name = tag->tag,
		};

		/* find the right subindex, or... */
		sub = avl_find(index, &key, &where);
		if (!sub) {
			/* ...allocate one if it doesn't exist */
			sub = malloc(sizeof(struct post_subindex));
			if (!sub)
				return ENOMEM;

			sub->name = str_getref(tag->tag);
			init_index_tree(&sub->subindex);

			avl_insert(index, sub, where);
		}

		/* allocate & add a entry to the subindex */
		tag_entry = malloc(sizeof(struct post_index_entry));
		if (!tag_entry)
			return ENOMEM;

		tag_entry->time = post->time;
		tag_entry->id   = post->id;
		tag_entry->post = post_getref(post);

		ASSERT3P(safe_avl_add(&sub->subindex, tag_entry), ==, NULL);
	}

	return 0;
}

int index_insert_post(struct post *post)
{
	struct post_index_entry *global;
	struct post_index_entry *by_time;
	int ret;

	/* allocate an entry for the global index */
	global = malloc(sizeof(struct post_index_entry));
	if (!global) {
		ret = ENOMEM;
		goto err;
	}

	global->time = 0;
	global->id   = post->id;
	global->post = post_getref(post);

	/* allocate an entry for the by-time index */
	by_time = malloc(sizeof(struct post_index_entry));
	if (!by_time) {
		ret = ENOMEM;
		goto err_free;
	}

	by_time->time = post->time;
	by_time->id   = post->id;
	by_time->post = post_getref(post);

	/*
	 * Now the fun begins.
	 */

	MXLOCK(&index_lock);

	/* add the post to the global index */
	if (safe_avl_add(&index_global, global)) {
		MXUNLOCK(&index_lock);
		ret = EEXIST;
		goto err_free_by_time;
	}

	/* add the post to the by-time index */
	ASSERT3P(safe_avl_add(&index_by_time, by_time), ==, NULL);

	ret = __insert_post_tags(&index_by_tag, post, &post->tags);
	if (ret)
		goto err_free_tags;

	ret = __insert_post_tags(&index_by_cat, post, &post->cats);
	if (ret)
		goto err_free_cats;

	MXUNLOCK(&index_lock);

	return 0;

err_free_cats:
	// XXX: __remove_post_tags(&index_by_cat, &post->cats);

err_free_tags:
	// XXX: __remove_post_tags(&index_by_tag, &post->tags);

	avl_remove(&index_by_time, by_time);

	MXUNLOCK(&index_lock);

err_free_by_time:
	post_putref(by_time->post);
	free(by_time);

err_free:
	post_putref(global->post);
	free(global);

err:
	return ret;
}

void revalidate_all_posts(void *arg)
{
	struct post_index_entry *cur;

	MXLOCK(&index_lock);
	avl_for_each(&index_global, cur)
		revalidate_post(cur->post);
	MXUNLOCK(&index_lock);
}

void index_for_each_tag(int (*init)(void *, unsigned long),
			void (*step)(void *, const struct str *, unsigned long,
				     unsigned long, unsigned long),
			void *private)
{
	struct post_subindex *tag;
	unsigned long cmin, cmax;
	int ret;

	if (!init && !step)
		return;

	MXLOCK(&index_lock);

	if (init) {
		ret = init(private, avl_numnodes(&index_by_tag));
		if (ret)
			goto err;
	}

	if (!step)
		goto err;

	/*
	 * figure out the minimum and maximum counts of tags
	 */
	cmin = ~0;
	cmax = 0;
	avl_for_each(&index_by_tag, tag) {
		cmin = MIN(cmin, avl_numnodes(&tag->subindex));
		cmax = MAX(cmax, avl_numnodes(&tag->subindex));
	}

	/*
	 * finally, invoke the step callback for each tag
	 */
	avl_for_each(&index_by_tag, tag)
		step(private, tag->name, avl_numnodes(&tag->subindex),
		     cmin, cmax);

err:
	MXUNLOCK(&index_lock);
}

static void __free_index(avl_tree_t *tree)
{
	struct post_index_entry *cur;
	void *cookie;

	cookie = NULL;
	while ((cur = avl_destroy_nodes(tree, &cookie))) {
		post_putref(cur->post);
		free(cur);
	}

	avl_destroy(tree);
}

static void __free_tag_index(avl_tree_t *tree)
{
	struct post_subindex *cur;
	void *cookie;

	cookie = NULL;
	while ((cur = avl_destroy_nodes(tree, &cookie))) {
		__free_index(&cur->subindex);
		str_putref((struct str *) cur->name);
		free(cur);
	}

	avl_destroy(tree);
}

void free_all_posts(void)
{
	MXLOCK(&index_lock);

	__free_index(&index_global);
	__free_index(&index_by_time);

	__free_tag_index(&index_by_tag);
	__free_tag_index(&index_by_cat);

	MXUNLOCK(&index_lock);
}
