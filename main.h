#ifndef __MAIN_H
#define __MAIN_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "post.h"
#include "vars.h"
#include "val.h"
#include "req.h"

extern int blahg_archive(struct req *req, int m, int paged);
extern int blahg_category(struct req *req, char *cat, int page);
extern int blahg_tag(struct req *req, char *tag, int paged);
extern int blahg_comment(struct req *req);
extern int blahg_index(struct req *req, int paged);
extern int blahg_story(struct req *req, int p, bool preview);
extern int blahg_admin(struct req *req);
#ifdef USE_XMLRPC
extern int blahg_pingback();
#endif

extern int R404(struct req *req, char *tmpl);
extern int R301(struct req *req, const char *url);

#endif
