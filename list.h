#ifndef __LIST_H
#define __LIST_H

#include <stddef.h>

#ifndef container_of
#define container_of(ptr, type, member) ({                      \
		const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
		(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

struct list_head {
	struct list_head *prev;
	struct list_head *next;
};

#define LIST(name)		struct list_head name
#define LIST_INIT(name)		LIST(name) = { &(name), &(name) };

static inline void INIT_LIST_HEAD(struct list_head *head)
{
	head->prev = head;
	head->next = head;
}

static inline int list_empty(struct list_head *head)
{
	return (head->prev == head);
}

static inline void list_add(struct list_head *new,
			     struct list_head *head)
{
	new->next = head->next;
	new->prev = head;
	head->next = new;
	new->next->prev = new;
}

static inline void list_add_tail(struct list_head *new,
				  struct list_head *head)
{
	new->next = head;
	new->prev = head->prev;
	head->prev = new;
	new->prev->next = new;
}

static inline void list_del(struct list_head *node)
{
	node->next->prev = node->prev;
	node->prev->next = node->next;

	node->next = node;
	node->prev = node;
}

static inline void list_splice(struct list_head *src,
				struct list_head *dst)
{
	src->next->prev = dst;
	src->prev->next = dst->next;
	dst->next->prev = src->prev;
	dst->next = src->next;
	INIT_LIST_HEAD(src);
}

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_first_entry(head, type, member) \
	container_of((head)->next, type, member)

#define list_last_entry(head, type, member) \
	container_of((head)->prev, type, member)

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; \
	     pos != (head); \
	     pos = n, n = n->next)

#define list_for_each_entry_safe(pos, n, head, member) \
	for (pos = list_entry((head)->next, typeof(*pos), member), \
	     n = list_entry(pos->member.next, typeof(*pos), member); \
	     &pos->member != (head); \
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

#endif
