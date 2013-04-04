/*
 * Copyright (c) 2011 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __AVL_H
#define __AVL_H

#include <stdlib.h>
#include <stddef.h>

#define container_of(ptr, type, member) ({                      \
		const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
		(type *)( (char *)__mptr - offsetof(type,member) );})

struct avl_node {
	struct avl_node *left;
	struct avl_node *right;
	int bal;
};

struct avl_root {
	struct avl_node *root;
	int (*cmp)(struct avl_node *, struct avl_node *);
	int dups;			/* allow duplicate keys? */
};

#define AVL_ROOT(r,c,d)		struct avl_root r = { .root = NULL, .cmp = (c), .dups = (d), }

#define AVL_ROOT_INIT(r,c,d)	do { \
					(r)->root = NULL; \
					(r)->cmp  = (c); \
					(r)->dups = (d); \
				} while(0)
#define AVL_NODE_INIT(n)	do { \
					(n)->left = NULL; \
					(n)->right = NULL; \
					(n)->bal = 0; \
				} while(0)

extern struct avl_node *avl_find_node(struct avl_root *root,
				      struct avl_node *key);

static inline struct avl_node **__avl_find_minimum(struct avl_node **node)
{
	for(;(*node)->left; node=&(*node)->left)
		;

	return node;
}

static inline struct avl_node *avl_find_minimum(struct avl_root *root)
{
	if (!root || !root->root)
		return NULL;

	return *__avl_find_minimum(&root->root);
}

static inline struct avl_node **__avl_find_maximum(struct avl_node **node)
{
	for(;(*node)->right; node=&(*node)->right)
		;

	return node;
}

static inline struct avl_node *avl_find_maximum(struct avl_root *root)
{
	if (!root || !root->root)
		return NULL;

	return *__avl_find_maximum(&root->root);
}

extern int __avl_insert_node(struct avl_root *root,
			     struct avl_node **_cur,
			     struct avl_node *new_node,
			     int *change);

static inline int avl_insert_node(struct avl_root *root,
				  struct avl_node *new_node)
{
	int change;

	/* new nodes start with 0 balance */
	AVL_NODE_INIT(new_node);

	/* empty tree - make the node the root */
	if (!root->root) {
		root->root = new_node;
		return 0;
	}

	change = 0;
	return __avl_insert_node(root, &root->root, new_node, &change);
}

extern struct avl_node **__avl_remove_node(struct avl_root *root,
					   struct avl_node **_cur,
					   struct avl_node *node,
					   int *change);

static inline void avl_remove_node(struct avl_root *root,
				   struct avl_node *node)
{
	int change;

	if (!root->root)
		return;

	change = 0;
	__avl_remove_node(root, &root->root, node, &change);
}

extern void avl_for_each(struct avl_root *root, int (*fn)(struct avl_node*, void*),
			 void *data, struct avl_node *start, struct avl_node *end);

#endif
