#ifndef __ERROR_H
#define __ERROR_H

#include <syslog.h>

#define LOG(...)	\
	syslog(LOG_LOCAL0 | LOG_CRIT, __VA_ARGS__)

#endif
