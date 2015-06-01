#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <door.h>
#include <stropts.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <alloca.h>

#include "error.h"
#include "math.h"
#include "val.h"
#include "str.h"
#include "mx.h"
#include "config.h"

static pthread_mutex_t lock;

static void process(void *cookie, char *argp, size_t argsz, door_desc_t *dp,
		    uint_t ndesc)
{
	struct str *in;
	struct str *out;
	char *str;

	MXLOCK(&lock);

	printf("arg: '%s'\n", argp);

	in = STR_DUP(argp);

	out = render_math(in);

	str = alloca(strlen(out->str) + 1);

	strcpy(str, out->str);

	str_putref(out);

	printf("out: '%s'\n", str);

	MXUNLOCK(&lock);

	door_return(str, strlen(str) + 1, NULL, 0);
}

static int setup_door(void)
{
	int tmp;
	int fd;

	fd = door_create(process, NULL, DOOR_NO_CANCEL);
	if (fd < 0) {
		LOG("door_create failed: %s (%d)", strerror(errno), errno);
		return errno;
	}

	fdetach(MATHD_DOOR_PATH);

	tmp = creat(MATHD_DOOR_PATH, S_IRUSR | S_IWUSR);
	if (tmp < 0) {
		LOG("creat failed: %s (%d)", strerror(errno), errno);
		return errno;
	}

	close(tmp);

	if (fattach(fd, MATHD_DOOR_PATH) < 0) {
		LOG("fattach failed: %s (%d)", strerror(errno), errno);
		return errno;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret;

	openlog("mathd", LOG_NDELAY | LOG_PID, LOG_LOCAL0);

	init_math(false);
	init_str_subsys();
	init_val_subsys();

	MXINIT(&lock);

	ret = setup_door();
	if (ret)
		goto out;

	for (;;)
		sleep(3600);

out:
	return ret;
}
