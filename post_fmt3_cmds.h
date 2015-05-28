#ifndef __POST_FMT3_CMDS_H
#define __POST_FMT3_CMDS_H

#include "post.h"

extern struct str *process_cmd(struct post *post, struct str *cmd,
			       struct str *txt, struct str *opt);

#endif
