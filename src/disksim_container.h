/*
 * disksim_container.h
 *
 *  Created on: Jun 2, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_CONTAINER_H_
#define DISKSIM_CONTAINER_H_

#include "disksim_list.h"

typedef struct hash_node {
	int key;
	void *value;
	struct hash_node *next;
} hash_node_t;

typedef struct hash_table {
	int order, mask;
	struct hash_node **slots;
} hash_table_t;

void  ht_create(hash_table_t *ht, int order);
void  ht_insert(hash_table_t *ht, int key, void *value);
void  ht_remove(hash_table_t *ht, int key);
int   ht_contains(hash_table_t *ht, int key);
void* ht_getvalue(hash_table_t *ht, int key);

#endif /* DISKSIM_CONTAINER_H_ */
