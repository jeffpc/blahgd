#include "error.h"

#include <sys/debug.h>
#include <syslog.h>
#include <stdarg.h>

void __my_log(const char *fmt, ...)
{
	va_list ap;
	char msg[512];

	va_start(ap, fmt);

	vsnprintf(msg, sizeof(msg), fmt, ap);

	if (!getenv("BLAHG_DISABLE_SYSLOG"))
		syslog(LOG_LOCAL0 | LOG_CRIT, "%s", msg);

	va_end(ap);
}

int __my_assfail(const char *a, const char *f, int l)
{
	LOG("assertion failed: %s, file: %s, line: %d", a, f, l);

	return assfail(a, f, l);
}

int __my_assfail3(const char *a, uintmax_t lv, const char *op, uintmax_t rv,
		  const char *f, int l)
{
	char msg[512];

	snprintf(msg, sizeof(msg), "%s (0x%lx %s 0x%lx)", a, lv, op, rv);

	LOG("assertion failed: %s, file: %s, line: %d", msg, f, l);

	return assfail(msg, f, l);
}
