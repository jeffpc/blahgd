#ifndef __STATIC_H
#define __STATIC_H

#include "req.h"

enum uri_type {
	URI_DYNAMIC,
	URI_STATIC,
	URI_BAD,
};

extern enum uri_type get_uri_type(const char *path);
extern int blahg_static(struct req *req);

#endif
