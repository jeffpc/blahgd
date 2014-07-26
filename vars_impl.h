#ifndef __VARS2_IMPL_H
#define __VARS2_IMPL_H

#include "vars.h"

#define SCOPE(vars, scope)	((vars)->scopes[scope])
#define C(vars)			SCOPE((vars), (vars)->cur)

#endif
