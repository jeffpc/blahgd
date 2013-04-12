#include "main.h"

int blahg_admin(struct req *req)
{
	req_head(req, "Content-Type", "text/plain");

	return 0;
}
