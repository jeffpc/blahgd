#include <stdlib.h>
#include <stdio.h>

#include "fsm.h"

void fsm_process(struct fsm *fsm)
{
	fsm_table_entry_t f;
	int tmp, ret = 0;

	for(;;) {
		tmp = fsm->inp_fxn(fsm);
		if (tmp == -1)
			break;

		f = fsm->state_table[fsm->state].action_table[tmp];
		if (!f)
			f = fsm->state_table[fsm->state].default_action;

		if (f)
			ret = f(fsm, tmp);
		if (ret == -1)
			break;
	}
}
