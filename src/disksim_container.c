/*
 * disksim_container.c
 *
 *  Created on: Jun 2, 2014
 *      Author: zyz915
 */

#include <stdlib.h>
#include <string.h>

#include "disksim_container.h"
#include "disksim_malloc.h"

extern int hn_idx;

void ht_create(hash_table_t *ht, int order)
{
	ht->order = order;
	ht->mask = (1 << order) - 1;
	ht->slots = (hash_node_t**) malloc(sizeof(void*) << order);
	memset(ht->slots, 0, sizeof(void*) << order);
}

void ht_insert(hash_table_t *ht, int key, void *value)
{
	int offset = (key ^ (key >> ht->order)) & ht->mask;
	hash_node_t *node;
	for (node = ht->slots[offset]; node != NULL; node = node->next)
		if (node->key == key) {
			node->value = value;
			return;
		}
	node = (hash_node_t*) disksim_malloc(hn_idx);
	node->key = key;
	node->value = value;
	node->next = ht->slots[offset];
	ht->slots[offset] = node;
}

void ht_remove(hash_table_t *ht, int key)
{
	int offset = (key ^ (key >> ht->order)) & ht->mask;
	hash_node_t *node, **prev = ht->slots + offset;
	for (node = ht->slots[offset]; node != NULL; node = node->next) {
		if (node->key == key) {
			(*prev) = node->next;
			disksim_free(hn_idx, node);
			return;
		}
		prev = &(node->next);
	}
}

int ht_contains(hash_table_t *ht, int key)
{
	int offset = (key ^ (key >> ht->order)) & ht->mask;
	hash_node_t *node, *prev = NULL;
	for (node = ht->slots[offset]; node != NULL; node = node->next)
		if (node->key == key)
			return 1;
	return 0;
}

void* ht_getvalue(hash_table_t *ht, int key)
{
	int offset = (key ^ (key >> ht->order)) & ht->mask;
	hash_node_t *node, *prev = NULL;
	for (node = ht->slots[offset]; node != NULL; node = node->next)
		if (node->key == key)
			return node->value;
	return NULL;
}
