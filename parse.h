#ifndef __PARSE_H
#define __PARSE_H

#include "main.h"

struct parser_output {
	struct req *req;

	void *scanner;
	char *output;

	char *input;
	int len;
	int pos;
};

#endif
