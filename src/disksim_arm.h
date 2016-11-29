/*
 * disksim_arm.h
 *
 *  Created on: Jul 19, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_ARM_H_
#define DISKSIM_ARM_H_

#include "disksim_erasure.h"
#include "disksim_req.h"

#define ARM_NORMAL	0
#define ARM_STATIC	1
#define ARM_DYNAMIC	2

typedef void(*arm_complete_t)(double time);
typedef void(*arm_internal_t)(double time, void *ctx);

typedef struct arm_struct {
	int method;
	int max_stripes;

	metadata_t *meta;
	//arm_internal_t internal_fn;
	arm_complete_t complete_fn;

	int patterns;

	struct list_head waitreqs;

	int ***cache;  // store best results
	int *sort;

	int *map, *distr; // temporary arrays
} arm_t;

void arm_init(arm_t *arm, int method, int max_sectors, int patterns,
		metadata_t *meta, arm_internal_t internal, arm_complete_t complete);

void arm_run(double time, arm_t *arm);

void arm_set_disk_failure(double time, arm_t *arm, int disk);

void arm_set_disk_repaired(double time, arm_t *arm, int disk);

const char* arm_get_method_name(int method);

//void arm_internal_event(arm_t *arm, double time, void* ctx);

#endif /* DISKSIM_ARM_H_ */
