/*
 * disksim_arm.c
 *
 *  Created on: Jul 19, 2014
 *      Author: zyz915
 */

#include <stdlib.h>
#include <string.h>

#include "disksim_arm.h"
#include "disksim_global.h"

extern int sb_idx, sh_idx;

static void arm_calculate(arm_t *arm, int disk, int *ptr, stripe_head_t *sh)
{
	metadata_t *meta = arm->meta;
	int pat = *ptr; // pattern bitmap
	if (pat == -1) {
		ptr[1] = ptr[2] = 0x3fffffff;
		return;
	}
	int *map = arm->map, *distr = arm->distr;
	int nr_total = meta->w * meta->n;
	int r, c, uid;
	memset(map, 0, sizeof(int) * nr_total);
	memset(distr, 0, sizeof(int) * meta->n);
	for (r = 0; r < meta->w; r++) {
		uid = ID(r, disk);
		if (meta->map_p2[uid][1 & (pat >> r)] == -1) // only consider 0/1
			pat ^= (1 << r);
		element_t *elem = meta->chains[meta->map_p2[uid][1 & (pat >> r)]];
		for (; elem; elem = elem->next)
			if (elem->col != disk)
				map[ID(elem->row, elem->col)] = 1;
	}
	ptr[0] = pat; // return actual pattern
	for (uid = 0; uid < nr_total; uid++)
		if (map[uid])
			distr[uid % meta->n] += meta->usize * (1 - sh->page[uid].state);
	int *cnt = meta->sctlr->count, maxi = 0, sum = 0;
	for (c = 0; c < meta->n; c++) { // c is logical
		maxi = max(maxi, distr[c] + cnt[(c + sh->stripeno) % meta->n]);
		sum += distr[c];
	}
	ptr[1] = sum;
	ptr[2] = maxi;
}

static void arm_make_request(double time, arm_t *arm)
{
	if (sh_has_next_stripe(arm->meta->sctlr)) {
		sub_ioreq_t *subreq = (sub_ioreq_t*) disksim_malloc(sb_idx);
		subreq->time = time;
		subreq->reqtype = REQ_TYPE_RECOVERY;
		subreq->state = STATE_BEGIN;
		subreq->stripeno = sh_get_next_stripe(arm->meta->sctlr);
		subreq->meta = (void*) arm;
		subreq->reqctx = NULL;
		sh_get_active_stripe(time, arm->meta->sctlr, subreq);
	}
}

static void arm_complete(double time, sub_ioreq_t *subreq, stripe_head_t *sh)
{
	arm_t *arm = (arm_t*) subreq->meta;
	if (!(--subreq->out_reqs)) {
		//arm->internal_fn(time, NULL);
		if (subreq->state == STATE_WRITE) {
			sh_release_stripe(time, arm->meta->sctlr, subreq->stripeno);
			disksim_free(sb_idx, subreq);
			sh_complete_stripe(arm->meta->sctlr, subreq->stripeno);
			if (sh_is_recovery_completed(arm->meta->sctlr)) {
				arm->complete_fn(time);
			} else
				arm_make_request(time, arm);
		} else
			sh_redo_callback(time, arm->meta->sctlr, subreq, sh);
	}
}

static void arm_do_recovery(double time, stripe_head_t *sh, sub_ioreq_t *subreq, int disk, int pattern)
{
	subreq->state += 1;
	subreq->out_reqs = 1;
	arm_t *arm = (arm_t*) subreq->meta;
	metadata_t *meta = arm->meta;
	stripe_ctlr_t *sctlr = meta->sctlr;
	int uid, r, nr_total = meta->w * meta->n, *map = arm->map;
	memset(map, 0, sizeof(int) * nr_total);
	if (subreq->state == STATE_READ) {
		for (r = 0; r < meta->w; r++) {
			int ch = 1 & (pattern >> r);
			uid = ID(r, disk);
			assert(meta->map_p2[uid][ch] != -1);
			element_t *elem = meta->chains[meta->map_p2[uid][ch]];
			for (; elem; elem = elem->next)
				if (elem->col != disk)
					map[ID(elem->row, elem->col)] = 1;
		}
	} else {
		for (r = 0; r < meta->w; r++)
			map[ID(r, disk)] = 1;
	}
	int flag = (subreq->state == STATE_READ ? DISKSIM_READ : DISKSIM_WRITE);
	for (uid = 0; uid < nr_total; uid++)
		if (map[uid]) {
			sh_request_t *shreq = (sh_request_t*) disksim_malloc(sh_idx);
			shreq->time = time;
			shreq->flag = flag;
			shreq->stripeno = subreq->stripeno;
			shreq->devno = uid % meta->n;
			shreq->blkno = uid / meta->n;
			shreq->reqctx = (void*) subreq;
			shreq->meta = (void*) sctlr;
			subreq->out_reqs += 1;
			sh_request_arrive(time, sctlr, sh, shreq);
		}
	arm_complete(time, subreq, sh);
}

static int arm_mdrr(int c, int p)
{
	int r, ret = 0;
	for (r = 0; r < ((p - 3) >> 1); r++)
		ret ^= (r & 1) << r;
	for (r = (p - 3) >> 1; r < p; r++)
		ret ^= (!(r & 1)) << r;
	return ret;
}

static int arm_rdor(int c, int p)
{
	int sp = 0, nsp, i, w;
	for (i = 1; i < p; i++)
		sp = sp | (1 << i * i % p);
	nsp = ((1 << p) - 2) & ~sp;

	if (c == p - 1) {
		w = 0;
	}  else if (c == p) {
		w = (1 << p) - 1;
	} else if ((1 & (sp >> c)) ^ ((p & 3) == 1)) {
		w = ((nsp >> (c + 1)) | nsp << (p - (c + 1)));
	} else {
		w = ((sp >> (c + 1)) | sp << (p - (c + 1)));
	}
	w &= (1 << p) - 1;
	return w;
}

static int arm_classify(int *distr, int n)
{
	int ret = n, i, maxi = 0;
	for (i = 0; i < n; i++)
		if (distr[i] > maxi) {
			maxi = distr[i];
			ret = i;
		}
	return ret;
}

static int arm_cmp(const void *a, const void *b)
{
	return (((int*)a)[1] != ((int*)b)[1]) ? ((int*)a)[1] - ((int*)b)[1] : ((int*)a)[2] - ((int*)b)[2];
}

static void arm_recovery(double time, sub_ioreq_t *subreq, stripe_head_t *sh, int fails, int *fd)
{
	arm_t *arm = (arm_t*) subreq->meta;
	int pattern = 0;
	int i, f_disk = 0, mask = (1 << arm->meta->w) - 1;
	while (!fd[f_disk]) ++f_disk;
	int *cnt = arm->meta->sctlr->count, n = arm->meta->n;
	assert(fails == 1 && (f_disk + sh->stripeno) % n == 0);
	int c = arm_classify(cnt, n); // physical
	if (c != n)
		c = (c + n - sh->stripeno % n) % n; // logical
	int *cache = arm->cache[f_disk][c];
	if (arm->method == ARM_NORMAL)
		arm_do_recovery(time, sh, subreq, f_disk, pattern);
	else if (arm->method == ARM_STATIC) {
		if (arm->meta->codetype == CODE_RDP)
			pattern = arm_rdor(f_disk, arm->meta->pr);
		if (arm->meta->codetype == CODE_XCODE)
			pattern = arm_mdrr(f_disk, arm->meta->pr);
		arm_do_recovery(time, sh, subreq, f_disk, pattern);
	} else {
		int ptr = 0, fill = 0, patts = arm->patterns;
		for (i = 0; i < patts; i++) {
			arm->sort[ptr] = cache[i];
			arm_calculate(arm, f_disk, arm->sort + ptr, sh);
			ptr += 3;
		}
		for (i = 0; i < patts; i++) {
			arm->sort[ptr] = rand() & mask;
			arm_calculate(arm, f_disk, arm->sort + ptr, sh);
			ptr += 3;
		}
		if (arm->method == ARM_DYNAMIC) {
			int temp;
			for (i = 0; i < ptr; i += 3) {
				temp = arm->sort[i + 1];
				arm->sort[i + 1] = arm->sort[i + 2];
				arm->sort[i + 2] = temp;
			}
		}
		int hf_patts = patts / 2;
		qsort(arm->sort + 6 * hf_patts, patts, sizeof(int) * 3, arm_cmp);
		qsort(arm->sort + 3 * hf_patts, patts, sizeof(int) * 3, arm_cmp);
		qsort(arm->sort + 0 * hf_patts, patts, sizeof(int) * 3, arm_cmp);
		memset(cache, -1, sizeof(int) * patts);
		for (i = 0; fill < patts && i < ptr; i += 3)
			if (i == 0 || arm->sort[i] != arm->sort[i - 3])
				cache[fill++] = arm->sort[i];
		arm_do_recovery(time, sh, subreq, f_disk, arm->sort[0]);
	}
}

void arm_init(arm_t *arm, int method, int max_sectors, int patterns,
		metadata_t *meta, arm_internal_t internal, arm_complete_t complete)
{
	int i, j;
	arm->method = method;
	arm->meta = meta;
	arm->patterns = patterns;
	arm->map = (int*) malloc(sizeof(int) * meta->w * meta->n);
	arm->distr = (int*) malloc(sizeof(int) * meta->n);
	memset(arm->map, 0, sizeof(int) * meta->w * meta->n);
	memset(arm->distr, 0, sizeof(int) * meta->n);
	arm->cache = (int***) malloc(sizeof(void*) * meta->n);
	for (i = 0; i < meta->n; i++) {
		arm->cache[i] = (int**) malloc(sizeof(void*) * (meta->n + 1));
		for (j = 0; j < meta->n + 1; j++) {
			arm->cache[i][j] = (int*) malloc(sizeof(int) * patterns);
			memset(arm->cache[i][j], -1, sizeof(int) * patterns);
		}
	}
	arm->sort = (int*) malloc(sizeof(int) * patterns * 6); //
	arm->max_stripes = max_sectors / (meta->w * meta->usize);
	arm->meta->sctlr->max_stripes = arm->max_stripes;
	sh_set_rec_callbacks(meta->sctlr, arm_recovery, arm_complete);
	//arm->internal_fn = internal;
	arm->complete_fn = complete;
}

void arm_run(double time, arm_t *arm)
{
	arm->waitreqs.prev = &arm->waitreqs;
	arm->waitreqs.next = &arm->waitreqs;
	arm_make_request(time, arm);
}

void arm_set_disk_failure(double time, arm_t *arm, int disk)
{
	sh_set_disk_failure(time, arm->meta->sctlr, disk);
	sh_initialize_recovery(arm->meta->sctlr, arm->method);
}

void arm_set_disk_repaired(double time, arm_t *arm, int disk)
{
	sh_set_disk_failure(time, arm->meta->sctlr, disk);
}

const char* arm_get_method_name(int method)
{
	switch (method) {
	case ARM_NORMAL:
		return "nothing";
	case ARM_STATIC:
		return "static";
	case ARM_DYNAMIC:
		return "dynamic";
	default:
		return "WRONG METHOD!";
	}
}
