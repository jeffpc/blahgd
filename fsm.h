#ifndef __FSM_H
#define __FSM_H

typedef int(*fsm_table_entry_t)();

struct fsm_state {
	fsm_table_entry_t *action_table;
	fsm_table_entry_t default_action;
};

struct fsm {
	int state;
	struct fsm_state *state_table;
	int (*inp_fxn)(struct fsm *);
	void *private;
};

extern void fsm_process(struct fsm *fsm);

#endif
