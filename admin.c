#include "req.h"

int blahg_admin(struct req *req)
{
	req_head(req, "Content-Type", "text/plain");

	/*
	 * FIXME:
	 * Loop through data/pending-comments dir entries.  For each entry,
	 * output a table row that has all the information.  IOW,
	 *  - pending-comments subdir name
	 *  - dump the meta sql
	 *  - dump the post body
	 */

	return 0;
}
