/*
 * disksim_req.h
 *
 *  Created on: Jul 14, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_REQ_H_
#define DISKSIM_REQ_H_

#include <stdio.h>

#include "disksim_container.h"
#include "disksim_interface.h"
#include "disksim_list.h"
#include "disksim_malloc.h"

#define DEBUG(msg) {puts(msg); fflush(stdout);}

#define REQ_TYPE_NORMAL		0
#define REQ_TYPE_RECOVERY	1
#define REQ_TYPE_MIGRATION	2

enum req_state {
	STATE_BEGIN,
	STATE_READ,
	STATE_WRITE,
	STATE_DONE
};

typedef struct ioreq {
	double time;   // request start time
	int reqno;     // request number
	long int blkno;// request start location (in sectors)
	int bcount;    // request length (in sectors)
	int flag;      // request flag, defined in disksim_global.h
	int out_reqs;  // decrease to 0 means finished
} ioreq_t;

typedef struct sub_ioreq {
	double time;   // request start time
	int reqtype;   // request type (normal/recovery/migration)
	int stripeno;  // stripe number
	long int blkno;// start location in stripe (in sectors)
	int bcount;    // request length (in sectors)
	int flag;      // request flag
	int out_reqs;  // decrease to 0 means finished
	enum req_state state;
	void *reqctx, *meta;
} sub_ioreq_t;

typedef struct sh_request {
	double time;  // request start time
	int stripeno; // stripe number
	int devno;    // logical device number
	long int blkno; // unit number
	int flag;
	void *reqctx, *meta;
} sh_request_t;

typedef struct page {
	int state; // 0=empty, 1=filled
} page_t;

typedef struct stripe_head {
	int users; // active or inactive
	int stripeno; // stripe number (default -1)
	page_t *page;
	struct list_head list; // link to active or inactive queue
	struct stripe_ctlr *sctlr; // link to controller
} stripe_head_t;

typedef struct wait_request {
	sub_ioreq_t *subreq;
	struct list_head list; // wait list
} wait_req_t;

typedef void(*sh_callback_t)(double time, sub_ioreq_t *subreq, stripe_head_t *sh);
typedef void(*sh_callback2_t)(double time, sub_ioreq_t *subreq, stripe_head_t *sh, int fails, int *fd);

typedef struct stripe_ctlr {
	int nr_disks; // number of devices
	int nr_units; // number of units
	int u_size;   // unit size (in sectors)

	int fails;   // number of failed disks
	int rec_prog; // progress of recovery
	int max_stripes; // max number of stripes
	int method; // method (normal, static, dynamic)

	int *dev_failed; // failed devices
	int *log_failed; // failed logical devices

	struct list_head inactive; // inactive queue
	struct list_head waitreqs; // no enough stripe_head

	hash_table_t *ht;

	sh_callback_t io_mapreq_fn;   // stripe becomes active
	sh_callback2_t io_degraded_fn; // stripe becomes active
	sh_callback_t io_complete_fn; // finishes requests

	sh_callback2_t rec_req_fn;   // stripe becomes active
	sh_callback_t rec_comp_fn;  // finishes requests
	void *rec_ctx;

	int *count; // number of handling requests
} stripe_ctlr_t;

extern int rq_idx;

static inline ioreq_t* create_ioreq()
{
	return (ioreq_t*) disksim_malloc(rq_idx);
}

static inline void free_ioreq(ioreq_t *req)
{
	disksim_free(rq_idx, req);
}

void sh_init(stripe_ctlr_t *sctlr, int nr_disks, int nr_units, int u_size);
void sh_set_io_callbacks(stripe_ctlr_t *sctlr, sh_callback_t mapreq, sh_callback2_t degraded, sh_callback_t complete);
void sh_set_rec_callbacks(stripe_ctlr_t *sctlr, sh_callback2_t recovery, sh_callback_t complete);
void sh_set_disk_failure(double time, stripe_ctlr_t *sctlr, int devno);
void sh_set_disk_repaired(double time, stripe_ctlr_t *sctlr, int devno);

void sh_get_active_stripe(double time, stripe_ctlr_t *sctlr, sub_ioreq_t *subreq);
void sh_redo_callback(double time, stripe_ctlr_t *sctlr, sub_ioreq_t *subreq, stripe_head_t *sh);
void sh_release_stripe(double time, stripe_ctlr_t *sctlr, int stripeno);
void sh_request_arrive(double time, stripe_ctlr_t *sctlr, stripe_head_t *sh, sh_request_t *shreq);
void sh_request_complete(double time, struct disksim_request *dr);

/* add strategy */
void sh_initialize_recovery(stripe_ctlr_t *sctlr, int method);
int  sh_is_recovery_completed(stripe_ctlr_t *sctlr);
int  sh_has_next_stripe(stripe_ctlr_t *sctlr);

int  sh_get_next_stripe(stripe_ctlr_t *sctlr);
int  sh_is_stripe_recovered(stripe_ctlr_t *sctlr, int stripeno);
void sh_complete_stripe(stripe_ctlr_t *sctlr, int stripeno);
void sh_add_history(stripe_ctlr_t *sctlr, int stripeno);

#endif /* DISKSIM_REQ_H_ */
