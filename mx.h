#ifndef __MX_H
#define __MX_H

#include <pthread.h>

#include "error.h"

#define MXINIT(l)	ASSERT0(pthread_mutex_init((l), NULL))
#define MXDESTROY(l)	ASSERT0(pthread_mutex_destroy(l))
#define MXLOCK(l)	ASSERT0(pthread_mutex_lock(l))
#define MXUNLOCK(l)	ASSERT0(pthread_mutex_unlock(l))
#define CONDINIT(c)	ASSERT0(pthread_cond_init((c), NULL))
#define CONDDESTROY(c)	ASSERT0(pthread_cond_destroy(c))
#define CONDWAIT(c,m)	ASSERT0(pthread_cond_wait((c),(m)))
#define CONDSIG(c)	ASSERT0(pthread_cond_signal(c))
#define CONDBCAST(c)	ASSERT0(pthread_cond_broadcast(c))

#endif
