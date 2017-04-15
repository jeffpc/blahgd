/*
 * Copyright (c) 2015-2017 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#include <jeffpc/str.h>
#include <jeffpc/error.h>
#include <jeffpc/mem.h>

#include "post.h"
#include "iter.h"
#include "utils.h"

/*
 * Having a post structure is nice but we need to be able to find it to
 * benefit from caching.  To make matters worse, there are several different
 * ways we can be trying to look up a post.  To make this reasonably fast,
 * we want to keep a number of indices.  The most boring and most
 * straightforward index is the posts-by-post-id (aka. index_global).  The
 * remaining indices (index_*) order their contents by publication time
 * (resolving ties by post id).
 *
 * In addition to the global index, there are three other indices:
 *
 *   - by time (used for index & archive listing)
 *   - by tag (used for tag listing)
 *   - by category (used for category listing)
 *
 * To maximize code reuse, the by-tag and by-category trees contain struct
 * post_subindex nodes for each (unique) tag/category.  Those nodes contain
 * AVL trees of their own with struct post_index_entry elements mapping
 * <timestamp, post id> (much like the by time index) to a post.
 *
 * Because this isn't complex enough, we also keep a linked list of all the
 * tag entries rooted in the global index tree node.
 *
 *      +--------------------------------+
 *      | index_global AVL tree          |
 *      |+-------------------------+     |   +------+
 *      || post global index entry | ... |   | post |
 *  +--->|   by_tag  by_cat  post  |     |   +------+
 *  |   |+-----|-------|------|----+     |      ^
 *  |   +------|-------|------|----------+      |
 *  |          |       |      |                 |
 *  |          |       |      +-----------------+
 *  |          |       |
 *  | +--------+       +--------------------------+
 *  | |                                           |
 *  | | +-------------------------------------+   |
 *  | | |  index_by_tag AVL tree              |   |
 *  | | | +-----------------------------+     |   |
 *  | | | |  post subindex              |     |   |
 *  | | | | +-------------------------+ |     |   |
 *  | | | | |  subindex AVL tree      | |     |   |
 *  | | | | | +-----------------+     | |     |   |
 *  | +-----> |post index entry | ... | |     |   |
 *  |   | | | |  global  xref   |     | | ... |   |
 *  |   | | | +----|------|-----+     | |     |   |
 *  |   | | |      |      |           | |     |   |
 *  ^<-------------+      +---> ...   | |     |   |
 *  |   | | |                         | |     |   |
 *  |   | | +-------------------------+ |     |   |
 *  |   | +-----------------------------+     |   |
 *  |   +-------------------------------------+   |
 *  |                                             |
 *  | +-------------------------------------------+
 *  | |
 *  | | +-------------------------------------+
 *  | | |  index_by_cat AVL tree              |
 *  | | | +-----------------------------+     |
 *  | | | |  post subindex              |     |
 *  | | | | +-------------------------+ |     |
 *  | | | | |  subindex AVL tree      | |     |
 *  | | | | | +-----------------+     | |     |
 *  | +-----> |post index entry | ... | |     |
 *  |   | | | |  global  xref   |     | | ... |
 *  |   | | | +----|------|-----+     | |     |
 *  |   | | |      |      |           | |     |
 *  ^<-------------+      +---> ...   | |     |
 *  |   | | |                         | |     |
 *  |   | | +-------------------------+ |     |
 *  |   | +-----------------------------+     |
 *  |   +-------------------------------------+
 *  |
 *  |   +-------------------------+
 *  |   | index_by_time AVL tree  |
 *  |   |+------------------+     |
 *  |   || post index entry | ... |
 *  |   ||   global         |     |
 *  |   |+-----|------------+     |
 *  |   +------|------------------+
 *  |          |
 *  +----------+
 *
 */

enum entry_type {
	ET_TAG = 1,	/* global's by_tag list */
	ET_CAT,		/* global's by_cat list */
	ET_TIME,	/* index_by_time */
};

struct post_global_index_entry {
	avl_node_t node;

	/* key */
	unsigned int id;

	/* value */
	struct post *post;

	/* other useful cached data from struct post */
	unsigned int time;

	/* list of tags & cats associated with this post */
	struct list by_tag;
	struct list by_cat;
};

struct post_index_entry {
	avl_node_t node;

	struct post_global_index_entry *global;

	/*
	 * key:
	 *   ->global->time
	 *   ->global->id
	 */

	/*
	 * value:
	 *   ->global->post
	 */
	struct str *name;
	enum entry_type type;

	/* list node for global index tag/cat list */
	struct list_node xref;
};

struct post_subindex {
	avl_node_t index;

	/* key */
	const struct str *name;

	/* value */
	avl_tree_t subindex;
};

static avl_tree_t index_global;
static avl_tree_t index_by_time;
static avl_tree_t index_by_tag;
static avl_tree_t index_by_cat;

static struct lock index_lock;

static struct mem_cache *index_entry_cache;
static struct mem_cache *global_index_entry_cache;
static struct mem_cache *subindex_cache;

/*
 * Assorted comparators
 */

/* compare two post global index entries by id */
static int post_global_index_cmp(const void *va, const void *vb)
{
	const struct post_global_index_entry *a = va;
	const struct post_global_index_entry *b = vb;

	if (a->id < b->id)
		return -1;
	if (a->id > b->id)
		return 1;
	return 0;
}

/* compare two post index entries by the timestamp */
static int post_index_cmp(const void *va, const void *vb)
{
	const struct post_index_entry *a = va;
	const struct post_index_entry *b = vb;

	/* first, compare by time */
	if (a->global->time < b->global->time)
		return -1;
	if (a->global->time > b->global->time)
		return 1;

	/* resolve ties by comparing post id */
	if (a->global->id < b->global->id)
		return -1;
	if (a->global->id > b->global->id)
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
	avl_create(&index_global, post_global_index_cmp,
		   sizeof(struct post_global_index_entry),
		   offsetof(struct post_global_index_entry, node));

	init_index_tree(&index_by_time);

	/* set up the by-tag/category indexes */
	avl_create(&index_by_tag, post_tag_cmp, sizeof(struct post_subindex),
		   offsetof(struct post_subindex, index));
	avl_create(&index_by_cat, post_tag_cmp, sizeof(struct post_subindex),
		   offsetof(struct post_subindex, index));

	MXINIT(&index_lock);

	index_entry_cache = mem_cache_create("index-entry-cache",
					     sizeof(struct post_index_entry),
					     0);
	ASSERT(!IS_ERR(index_entry_cache));

	global_index_entry_cache = mem_cache_create("global-index-entry-cache",
						    sizeof(struct post_global_index_entry),
						    0);
	ASSERT(!IS_ERR(global_index_entry_cache));

	subindex_cache = mem_cache_create("subindex-cache",
					  sizeof(struct post_subindex), 0);
	ASSERT(!IS_ERR(subindex_cache));
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
	struct post_global_index_entry *ret;
	struct post_global_index_entry key = {
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

	/* if there is no tree, there are no posts */
	if (!tree) {
		MXUNLOCK(&index_lock);
		return 0;
	}

	/* skip over the first entries as requested */
	for (cur = avl_last(tree); cur && skip; cur = AVL_PREV(tree, cur))
		skip--;

	/* get a reference for every post we're returning */
	for (i = 0; cur && nposts; cur = AVL_PREV(tree, cur)) {
		if (pred && !pred(cur->global->post, private))
			continue;

		ret[i] = post_getref(cur->global->post);

		nposts--;
		i++;
	}

	MXUNLOCK(&index_lock);

	return i;
}

static int __insert_post_tags(avl_tree_t *index,
			      struct post_global_index_entry *global,
			      avl_tree_t *taglist, struct list *xreflist,
			      enum entry_type type)
{
	struct post_index_entry *tag_entry;
	struct post_subindex *sub;
	struct post_tag *tag;
	avl_index_t where;

	avl_for_each(taglist, tag) {
		struct post_subindex key = {
			.name = tag->tag,
		};

		/* find the right subindex, or... */
		sub = avl_find(index, &key, &where);
		if (!sub) {
			/* ...allocate one if it doesn't exist */
			sub = mem_cache_alloc(subindex_cache);
			if (!sub)
				return -ENOMEM;

			sub->name = str_getref(tag->tag);
			init_index_tree(&sub->subindex);

			avl_insert(index, sub, where);
		}

		/* allocate & add a entry to the subindex */
		tag_entry = mem_cache_alloc(index_entry_cache);
		if (!tag_entry)
			return -ENOMEM;

		tag_entry->global = global;
		tag_entry->name   = str_getref(tag->tag);
		tag_entry->type   = type;

		ASSERT3P(safe_avl_add(&sub->subindex, tag_entry), ==, NULL);
		list_insert_tail(xreflist, tag_entry);
	}

	return 0;
}

int index_insert_post(struct post *post)
{
	struct post_global_index_entry *global;
	struct post_index_entry *by_time;
	int ret;

	/* allocate an entry for the global index */
	global = mem_cache_alloc(global_index_entry_cache);
	if (!global) {
		ret = -ENOMEM;
		goto err;
	}

	global->id   = post->id;
	global->post = post_getref(post);
	global->time = post->time;
	list_create(&global->by_tag, sizeof(struct post_index_entry),
		    offsetof(struct post_index_entry, xref));
	list_create(&global->by_cat, sizeof(struct post_index_entry),
		    offsetof(struct post_index_entry, xref));

	/* allocate an entry for the by-time index */
	by_time = mem_cache_alloc(index_entry_cache);
	if (!by_time) {
		ret = -ENOMEM;
		goto err_free;
	}

	by_time->global = global;
	by_time->name   = NULL;
	by_time->type   = ET_TIME;

	/*
	 * Now the fun begins.
	 */

	MXLOCK(&index_lock);

	/* add the post to the global index */
	if (safe_avl_add(&index_global, global)) {
		MXUNLOCK(&index_lock);
		ret = -EEXIST;
		goto err_free_by_time;
	}

	/* add the post to the by-time index */
	ASSERT3P(safe_avl_add(&index_by_time, by_time), ==, NULL);

	ret = __insert_post_tags(&index_by_tag, global, &post->tags,
				 &global->by_tag, ET_TAG);
	if (ret)
		goto err_free_tags;

	ret = __insert_post_tags(&index_by_cat, global, &post->cats,
				 &global->by_cat, ET_CAT);
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
	mem_cache_free(index_entry_cache, by_time);

err_free:
	post_putref(global->post);
	mem_cache_free(global_index_entry_cache, global);

err:
	return ret;
}

void revalidate_all_posts(void *arg)
{
	struct post_global_index_entry *cur;

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

static void __free_global_index(avl_tree_t *tree)
{
	struct post_global_index_entry *cur;
	void *cookie;

	cookie = NULL;
	while ((cur = avl_destroy_nodes(tree, &cookie))) {
		post_putref(cur->post);
		list_destroy(&cur->by_tag);
		list_destroy(&cur->by_cat);
		mem_cache_free(global_index_entry_cache, cur);
	}

	avl_destroy(tree);
}

static void __free_index(avl_tree_t *tree)
{
	struct post_index_entry *cur;
	void *cookie;

	cookie = NULL;
	while ((cur = avl_destroy_nodes(tree, &cookie))) {
		struct list *xreflist = NULL;

		switch (cur->type) {
			case ET_TAG:
				xreflist = &cur->global->by_tag;
				break;
			case ET_CAT:
				xreflist = &cur->global->by_cat;
				break;
			case ET_TIME:
				xreflist = NULL;
				break;
		}

		if (xreflist)
			list_remove(xreflist, cur);

		str_putref(cur->name);
		mem_cache_free(index_entry_cache, cur);
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
		mem_cache_free(subindex_cache, cur);
	}

	avl_destroy(tree);
}

void free_all_posts(void)
{
	MXLOCK(&index_lock);

	__free_tag_index(&index_by_tag);
	__free_tag_index(&index_by_cat);

	__free_index(&index_by_time);

	__free_global_index(&index_global);

	MXUNLOCK(&index_lock);
}
