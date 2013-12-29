#ifndef __POST_FMT3_CMDS_H
#define __POST_FMT3_CMDS_H

#include "post.h"

extern struct val *process_cmd(struct post *post, struct val *vcmd,
			       struct val *vtxt, struct val *vopt);

#endif
