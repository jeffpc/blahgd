/*
 * Copyright (c) 2009-2018 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
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

#ifndef __POST_H
#define __POST_H

#include <time.h>
#include <stdbool.h>

#include <jeffpc/synch.h>
#include <jeffpc/refcnt.h>
#include <jeffpc/list.h>
#include <jeffpc/rbtree.h>

#include "vars.h"

struct post_tag {
	struct rb_node node;
	struct str *tag;
};

struct comment {
	struct list_node list;
	unsigned int id;
	struct str *author;
	struct str *email;
	unsigned int time;
	struct str *ip;
	struct str *url;

	struct str *body;
};

struct post {
	refcnt_t refcnt;

	struct lock lock;

	bool preview;
	bool listed;

	/* from 'posts' table */
	unsigned int id;
	unsigned int time;
	struct str *title;
	unsigned int fmt;

	/* from 'post_tags' table */
	struct rb_tree tags;
	struct rb_tree cats;

	/* from 'comments' table */
	struct list comments;
	unsigned int numcom;

	/* body */
	struct str *body;

	struct str *twitter_img;

	/* filenames used to construct this post */
	struct nvlist *files;
};

struct req;

extern void init_post_subsys(void);
extern int post_add_filename(struct post *post, const char *path, uint64_t cache_rev);
extern struct post *load_post(int postid, bool preview);
extern void post_refresh(struct post *post);
extern void post_destroy(struct post *post);
extern void load_posts(struct req *req, struct post **posts, int nposts,
		       bool moreposts);
extern int load_all_posts(void);
extern void free_all_posts(void);
extern struct nvlist *get_post(struct req *req, int postid,
			       const char *titlevar, bool preview);

extern void init_post_index(void);
extern struct post *index_lookup_post(unsigned int postid);
extern int index_get_posts(struct post **ret, struct str *tagname,
			   bool tag, bool (*pred)(struct post *, void *),
			   void *private, int skip, int nposts);
extern void index_for_each_tag(int (*init)(void *, unsigned long),
			       void (*step)(void *, struct str *, unsigned long,
					    unsigned long, unsigned long),
			       void *private);
extern int index_insert_post(struct post *post);

REFCNT_INLINE_FXNS(struct post, post, refcnt, post_destroy, NULL)

static inline void post_lock(struct post *post)
{
	MXLOCK(&post->lock);
}

static inline void post_unlock(struct post *post)
{
	MXUNLOCK(&post->lock);
}

#define max(a,b)	((a)<(b)? (b) : (a))

#endif
