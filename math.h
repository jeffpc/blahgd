#ifndef __MATH_H
#define __MATH_H

#include <stdbool.h>

extern void init_math(bool daemonized);
extern struct str *render_math(struct str *val);

#endif
