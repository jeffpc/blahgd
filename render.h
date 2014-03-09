#ifndef __RENDER_H
#define __RENDER_H

#include "req.h"

extern char *render_template(struct req *req, const char *tmpl);
extern char *render_page(struct req *req, char *str);

#endif
