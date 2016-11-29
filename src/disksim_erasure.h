/*
 * disksim_erasure.h
 *
 *  Created on: Feb 22, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_ERASURE_H_
#define DISKSIM_ERASURE_H_

#include "disksim_req.h"

#define CODE_RDP		0	// RDP code
#define CODE_EVENODD	1	// EVENODD code
#define CODE_HCODE		2	// H-code
#define CODE_XCODE		3	// X-code
#define CODE_LIBERATION	4	// Liberation code
#define CODE_SHCODE		5	// Shortened H-code
#define CODE_EXT_HCODE	6	// Extended H-code
#define CODE_STAR		7	// STAR code
#define CODE_TRIPLE		8	// Triple Parity code
#define CODE_CODE56		9	// Code 5-6
#define CODE_PCODE		10	// P-code
#define CODE_CYCLIC		11	// Cyclic code
#define CODE_RAID0		12	// RAID-0
#define CODE_RAID5		13	// RAID-5
#define CODE_XICODE		14  // XI-code
#define CODE_HDD1		15  // HDD1
#define CODE_CRS		16  // Cauchy Reed-Solomon

#define ID(row, col) ((row) * meta->n + (col))

typedef void(*erasure_complete_t)(double time, ioreq_t *req);

typedef struct element {
	int row;
	int col;
	struct element *next;
} element_t;

typedef struct metadata {
	int codetype; // a unique id for each code
	int usize;    // unit size in sectors (512K)
	int n;  // number of logical disks
	int k;  // number of data disks
	int m;  // number of parity disks
	int w;  // word size
	int pr; // prime number if exists

	element_t **chains; // an array of m*w chains
	element_t *loc_d;   // map from data block ID to its location
	int **map_p;  // map from data ID to a list of parity IDs it affects
	int **map_p2; // map from any ID to a list of parity chains it is protected by

	long long bitmap;  // parity bitmap
	page_t *page; // internal calculation

	stripe_ctlr_t *sctlr;

	erasure_complete_t comp_fn;  // call this function when finishes a request
} metadata_t;

typedef int(*initializer_t)(struct metadata*);

typedef struct {
	int codeID;
	int level;        // number of maximum concurrent disk failures
	const char *flag; // command line spelling of the code (eg. rdp, hcode)
	const char *name; // name of the code (eg. RDP, H-code)
	initializer_t init; // initializer
} codespec_t;

const char* get_code_name(int code);
int get_code_id(const char *code);

// class level initialization
void erasure_initialize();
// code level initialization
void erasure_code_init(metadata_t *meta, int codetype, int disks, int usize, erasure_complete_t comp, int checkmode);
void erasure_handle_request(double time, metadata_t *meta, ioreq_t *req);
void erasure_maprequest(double time, sub_ioreq_t *subreq, stripe_head_t *sh);
void erasure_degraded(double time, sub_ioreq_t *subreq, stripe_head_t *sh, int fails, int *fd);
void erasure_iocomplete(double time, sub_ioreq_t *subreq, stripe_head_t *sh);

#endif /* DISKSIM_ERASURE_H_ */
