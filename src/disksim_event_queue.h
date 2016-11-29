/*
 * disksim_event_queue.h
 *
 *  Created on: Mar 26, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_EVENT_QUEUE_H_
#define DISKSIM_EVENT_QUEUE_H_

#include "disksim_global.h"
#include "disksim_malloc.h"

typedef struct event_node {
	double time;
	int type;
	int *ctx;

	struct event_node *left, *right;
	struct event_node *next;
	int depth;
} event_node_t;

typedef struct event_queue {
	event_node_t *root;
	int size;
} event_queue_t;

event_node_t* create_event(double time, int type, void *ctx);

void event_queue_init(event_queue_t *eq);
void event_queue_add(event_queue_t *eq, event_node_t *en);
event_node_t* event_queue_pop(event_queue_t *eq);

#endif /* DISKSIM_EVENT_QUEUE_H_ */
