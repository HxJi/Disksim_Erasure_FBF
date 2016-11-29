/*
 * disksim_req.c
 *
 *  Created on: Jul 14, 2014
 *      Author: zyz915
 */

#include <stdlib.h>
#include <string.h>

#include "disksim_global.h"
#include "disksim_malloc.h"
#include "disksim_interface.h"
#include "disksim_req.h"
#include "disksim_timer.h"

extern struct disksim_interface *interface;
extern int sh_idx, wt_idx, dr_idx;

void sh_init(stripe_ctlr_t *sctlr, int nr_disks, int nr_units, int u_size)
{
	int i;
	sctlr->ht = (hash_table_t*) malloc(sizeof(hash_table_t));
	ht_create(sctlr->ht, 10);
	sctlr->inactive.prev = sctlr->inactive.next = &sctlr->inactive;
	sctlr->nr_disks = nr_disks;
	sctlr->waitreqs.prev = sctlr->waitreqs.next = &sctlr->waitreqs;
	sctlr->nr_units = nr_units;
	sctlr->u_size = u_size;
	int nr_stripe_head = 128;
	for (i = 0; i < nr_stripe_head; i++) {
		stripe_head_t *sh = (stripe_head_t*) disksim_malloc(sh_idx);
		sh->users = 0;
		sh->stripeno = -1;
		sh->page = (page_t*) malloc(sizeof(page_t) * nr_units * nr_disks);
		list_add_tail(&(sh->list), &(sctlr->inactive));
	}
	sctlr->fails = 0; // normal
	sctlr->dev_failed = (int*) malloc(sizeof(int) * nr_disks);
	sctlr->log_failed = (int*) malloc(sizeof(int) * nr_disks);
	sctlr->count = (int*) malloc(sizeof(int) * nr_disks);
	memset(sctlr->dev_failed, 0, sizeof(int) * nr_disks);
	memset(sctlr->count, 0, sizeof(int) * nr_disks);
}

void sh_set_io_callbacks(stripe_ctlr_t *sctlr, sh_callback_t mapreq,
		sh_callback2_t degraded, sh_callback_t complete)
{
	sctlr->io_mapreq_fn = mapreq;
	sctlr->io_degraded_fn = degraded;
	sctlr->io_complete_fn = complete;
}

void sh_set_rec_callbacks(stripe_ctlr_t *sctlr, sh_callback2_t recovery, sh_callback_t complete)
{
	sctlr->rec_req_fn = recovery;
	sctlr->rec_comp_fn = complete;
}

void sh_set_disk_failure(double time, stripe_ctlr_t *sctlr, int devno) {
	if (!sctlr->dev_failed[devno]) {
		sctlr->dev_failed[devno] = 1;
		sctlr->fails += 1;
	}
}

void sh_set_disk_repaired(double time, stripe_ctlr_t *sctlr, int devno) {
	if (sctlr->dev_failed[devno]) {
		sctlr->dev_failed[devno] = 0;
		sctlr->fails -= 1;
	}
}

static void sh_return_stripe(double time, stripe_ctlr_t *sctlr, sub_ioreq_t *subreq, stripe_head_t *sh)
{
	int stripeno = subreq->stripeno;
	if (sctlr->fails) {
		int i, disks = sctlr->nr_disks;
		memset(sctlr->log_failed, 0, sizeof(int) * disks);
		for (i = 0; i < disks; i++)
			if (sctlr->dev_failed[i])
				sctlr->log_failed[(i + disks - stripeno % disks) % disks] = 1;
	}
	if (subreq->reqtype == REQ_TYPE_NORMAL) {
		if (sh_is_stripe_recovered(sctlr, stripeno))
			sctlr->io_mapreq_fn(time, subreq, sh);
		else
			sctlr->io_degraded_fn(time, subreq, sh, sctlr->fails, sctlr->log_failed);
	}
	if (subreq->reqtype == REQ_TYPE_RECOVERY)
		sctlr->rec_req_fn(time, subreq, sh, sctlr->fails, sctlr->log_failed);
}

void sh_get_active_stripe(double time, stripe_ctlr_t *sctlr, sub_ioreq_t *subreq)
{
	int stripeno = subreq->stripeno;
	if (ht_contains(sctlr->ht, stripeno)) {
		stripe_head_t *sh = (stripe_head_t*) ht_getvalue(sctlr->ht, stripeno);
		sh->users += 1;
		if (sh->users == 1) // remove from inactive queue
			list_del(&(sh->list));
		sh_return_stripe(time, sctlr, subreq, sh);
		return;
	}
	if (!list_empty(&(sctlr->inactive))) {
		stripe_head_t *sh = list_entry(sctlr->inactive.next, stripe_head_t, list);
		list_del(&(sh->list));
		if (sh->stripeno != -1) {
			ht_remove(sctlr->ht, sh->stripeno);
			memset(sh->page, 0, sizeof(page_t) * sctlr->nr_units * sctlr->nr_disks);
		}
		sh->stripeno = stripeno;
		ht_insert(sctlr->ht, stripeno, sh);
		sh->users += 1;
		sh_return_stripe(time, sctlr, subreq, sh);
		return;
	}
	// sleep
	wait_req_t *wait = (wait_req_t*) disksim_malloc(wt_idx);
	wait->subreq = subreq;
	list_add_tail(&(wait->list), &(sctlr->waitreqs));
}

void sh_redo_callback(double time, stripe_ctlr_t *sctlr, sub_ioreq_t *subreq, stripe_head_t *sh)
{
	sh_return_stripe(time, sctlr, subreq, sh);
}


void sh_release_stripe(double time, stripe_ctlr_t *sctlr, int stripeno)
{
	stripe_head_t *sh = (stripe_head_t*) ht_getvalue(sctlr->ht, stripeno);
	sh->users -= 1;
	if (sh->users == 0) {
		list_add_tail(&(sh->list), &(sctlr->inactive));
		if (!list_empty(&(sctlr->waitreqs))) {
			wait_req_t *wait = list_entry(sctlr->waitreqs.next, wait_req_t, list);
			list_del(&(wait->list));
			sh_get_active_stripe(time, sctlr, wait->subreq);
			disksim_free(wt_idx, wait);
		} else
			memset(sh->page, 0, sizeof(page_t) * sctlr->nr_units * sctlr->nr_disks);
	}
}

static void sh_send_request(double time, stripe_ctlr_t *sctlr, sh_request_t *shreq)
{
	struct disksim_request *dr = (struct disksim_request*) disksim_malloc(dr_idx);
	int base = (shreq->stripeno * sctlr->nr_units + shreq->blkno) * sctlr->u_size;
	dr->start = time;
	dr->flags = shreq->flag;
	dr->devno = (shreq->devno + shreq->stripeno) % sctlr->nr_disks;
	dr->blkno = base;
	dr->bytecount = sctlr->u_size * 512;
	dr->reqctx = (void*) shreq;
	sctlr->count[dr->devno] += dr->bytecount >> 9;
	disksim_interface_request_arrive(interface, time, dr);
}

void sh_request_arrive(double time, stripe_ctlr_t *sctlr, stripe_head_t *sh, sh_request_t *shreq)
{
	sub_ioreq_t *subreq = (sub_ioreq_t*) shreq->reqctx;
	int unit_id = shreq->blkno * sctlr->nr_disks + shreq->devno;
	page_t *pg = sh->page + unit_id;
	if (shreq->flag & DISKSIM_READ) {
		if (pg->state == 0) {
			sh_send_request(time, sctlr, shreq);
		} else
			sctlr->io_complete_fn(time, subreq, sh);
	} else {
		pg->state = 1;
		sh_send_request(time, sctlr, shreq);
	}
}

void sh_request_complete(double time, struct disksim_request *dr)
{
	sh_request_t *shreq = (sh_request_t*) dr->reqctx;
	sub_ioreq_t *subreq = (sub_ioreq_t*) shreq->reqctx;
	stripe_ctlr_t *sctlr = (stripe_ctlr_t*) shreq->meta;
	stripe_head_t *sh = (stripe_head_t*) ht_getvalue(sctlr->ht, subreq->stripeno);
	if (shreq->flag & DISKSIM_READ) {
		int unit_id = shreq->blkno * sctlr->nr_disks + shreq->devno;
		page_t *pg = sh->page + unit_id;
		pg->state = 1;
	}
	sctlr->count[dr->devno] -= dr->bytecount >> 9;
	if (subreq->reqtype == REQ_TYPE_NORMAL)
		sctlr->io_complete_fn(time, subreq, sh);
	if (subreq->reqtype == REQ_TYPE_RECOVERY)
		sctlr->rec_comp_fn(time, subreq, sh);
	disksim_free(sh_idx, shreq);
}

void sh_initialize_recovery(stripe_ctlr_t *sctlr, int method)
{
	sctlr->rec_prog = 0;
	sctlr->method = method;
}

int sh_is_recovery_completed(stripe_ctlr_t *sctlr)
{
	return (sctlr->rec_prog == sctlr->max_stripes);
}

int sh_has_next_stripe(stripe_ctlr_t *sctlr)
{
	return (sctlr->rec_prog < sctlr->max_stripes);
}

int sh_is_stripe_recovered(stripe_ctlr_t *sctlr, int stripeno)
{
	return stripeno < sctlr->rec_prog;
}

int sh_get_next_stripe(stripe_ctlr_t *sctlr)
{
	return sctlr->rec_prog;
}

void sh_complete_stripe(stripe_ctlr_t *sctlr, int stripeno)
{
	sctlr->rec_prog++;
}

void sh_add_history(stripe_ctlr_t *sctlr, int stripeno)
{
}
