/*
 * disksim_event_queue.c
 *
 *  Created on: Mar 26, 2014
 *      Author: zyz915
 */

#include <stdlib.h>

#include "disksim_event_queue.h"
#include "disksim_global.h"

extern int en_idx;

event_node_t* create_event(double time, int type, void *ctx) {
	event_node_t *ret = (event_node_t*) disksim_malloc(en_idx);
	ret->time = time;
	ret->type = type;
	ret->ctx = ctx;
	ret->depth = 1;
	ret->left = NULL;
	ret->right = NULL;
	return ret;
}

void event_queue_init(event_queue_t *eq) {
	eq->root = NULL;
	eq->size = 0;
}

static event_node_t* merge(event_node_t *a, event_node_t *b) {
	event_node_t *t;
	if (a == NULL) return b;
	if (a->time < b->time) {
		a->right = merge(a->right, b);
		if (!a->left || a->left->depth < a->right->depth) {
			t = a->left;
			a->left = a->right;
			a->right = t;
		}
		a->depth = a->left->depth + 1;
		return a;
	} else {
		b->right = merge(b->right, a);
		if (!b->left || b->left->depth < b->right->depth) {
			t = b->left;
			b->left = b->right;
			b->right = t;
		}
		b->depth = b->left->depth + 1;
		return b;
	}
}

void event_queue_add(event_queue_t *eq, event_node_t *en) {
	eq->root = merge(eq->root, en);
	eq->size += 1;
}

event_node_t* event_queue_pop(event_queue_t *eq) {
	if (eq->root == NULL)
		return NULL;
	event_node_t *ret = eq->root;
	eq->root = merge(ret->right, ret->left);
	eq->size -= 1;
	return ret;
}
