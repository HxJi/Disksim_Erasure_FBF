/*
 * disksim_erasure.c
 *
 *  Created on: Feb 22, 2014
 *	  Author: zyz915
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "disksim_global.h"
#include "disksim_erasure.h"
#include "disksim_malloc.h"
#include "disksim_req.h"
#include "disksim_reqflags.h"
#include "crs.h"

static codespec_t specs[32];
static int nr_codes = 0, code_id;

extern int el_idx, sb_idx, sh_idx;

const char* get_code_name(int code)
{
	for (code_id = 0; code_id < nr_codes; code_id++)
		if (code == specs[code_id].codeID)
			return specs[code_id].name;
	fprintf(stderr, "invalid code ID: %d\n", code);
	exit(-1);
	return "";
}

int get_code_id(const char *flag)
{
	for (code_id = 0; code_id < nr_codes; code_id++)
		if (!strcmp(flag, specs[code_id].flag))
			return specs[code_id].codeID;
	fprintf(stderr, "invalid code name: %s\n", flag);
	exit(-1);
	return -1;
}

static int check_prime(int number)
{
	int i;
	for (i = 2; i * i <= number; i++)
		if (number % i == 0)
			return 0;
	return 1;
}

static element_t* create_elem(int row, int col, element_t *next)
{
	if (col < 0) return next;
	element_t *ret = (element_t*) disksim_malloc(el_idx);
	ret->row = row;
	ret->col = col;
	ret->next = next;
	return ret;
}

static void print_chains(metadata_t *meta)
{
	int nr_parity = meta->w * meta->m, i;
	element_t *elem;
	for (i = 0; i < nr_parity; i++) {
		for (elem = meta->chains[i]; elem != NULL; elem = elem->next)
			printf(" (%d, %d)", elem->row, elem->col);
		printf("\n");
	}
	DEBUG("");
}

static int evenodd_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 2;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((r + p - c + 1) % p)
				elem = create_elem((r + p - c) % p, c, elem);
		for (c = 1; c < p; c++)
			elem = create_elem(p - 1 - c, c, elem);
		meta->chains[meta->w + r] = create_elem(r, p + 1, elem);
	}
	return 0;
}

static int rdp_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 1;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p - 1; c++)
			elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p - 1, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((r - c + p + 1) % p)
				elem = create_elem((r - c + p) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, p, elem);
	}
	return 0;
}

static int hcode_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 1;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if (r + 1 != c)
				elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((c + p - r - 1) % p)
				elem = create_elem((c + p - r - 2) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, r + 1, elem);
	}
	return 0;
}

static int xcode_initialize(metadata_t *meta) {
	int r, c, p = meta->pr = meta->n;
	if (!check_prime(p)) return -1;
	meta->w = p;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (c = 0; c < p; c++) {
		element_t *elem = NULL;
		for (r = 0; r < p - 2; r++)
			elem = create_elem(r, (r + c + 2) % p, elem);
		meta->chains[c] = create_elem(p - 2, c, elem);
	}
	for (c = 0; c < p; c++) {
		element_t *elem = NULL;
		for (r = 0; r < p - 2; r++)
			elem = create_elem(r, (c + p - r - 2) % p, elem);
		meta->chains[meta->w + c] = create_elem(p - 1, c, elem);
	}
	return 0;
}

static int liberation_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 2;
	if (!check_prime(p)) return -1;
	meta->w = p;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p, elem);
	}
	for (c = 1; c < p; c++) {
		int y = c * (p - 1) / 2 % p;
		meta->chains[meta->w + y] = create_elem((y + c - 1) % p, c, NULL);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = meta->chains[meta->w + r];
		for (c = 0; c < p; c++)
			elem = create_elem((r + c) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, p + 1, elem);
	}
	return 0;
}

static int ext_hcode_initialize(metadata_t *meta)
{
	int r, c, p = meta->n - 2;
	while (!check_prime(p)) p++;
	int sh = p + 2 - meta->n;
	meta->pr = p;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if (r + 1 != c)
				elem = create_elem(r, c - sh, elem);
		meta->chains[r] = create_elem(r, p - sh, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((c + p - r - 1) % p)
				elem = create_elem((c + p - r - 2) % p, c - sh, elem);
		meta->chains[meta->w + r] = create_elem(r, r + 1 - sh, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++) {
			int r1 = (r + p - c) % p;
			if (r1 + 1 != c)
				elem = create_elem((r1 + 1 == p ? c - 1 : r1), c - sh, elem);
			meta->chains[meta->w * 2 + r] = create_elem(r, p + 1 - sh, elem);
		}
	}
	return 0;
}

static int star_initialize(metadata_t *meta)
{
	int r, c, p = meta->n - 3;
	while (!check_prime(p)) p++;
	int sh = p + 3 - meta->n;
	meta->pr = p;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			elem = create_elem(r, c - sh, elem);
		meta->chains[r] = create_elem(r, p - sh, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 1; c < p; c++)
			elem = create_elem(p - 1 - c, c - sh, elem);
		for (c = 0; c < p; c++)
			if ((r + p - c + 1) % p)
				elem = create_elem((r + p - c) % p, c - sh, elem);
		meta->chains[meta->w + r] = create_elem(r, p + 1 - sh, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 1; c < p; c++)
			elem = create_elem(c - 1, c - sh, elem);
		for (c = 0; c < p; c++)
			if ((c + r + 1) % p)
				elem = create_elem((r + c) % p, c - sh, elem);
		meta->chains[meta->w * 2 + r] = create_elem(r, p + 2 - sh, elem);
	}
	return 0;
}

static int triple_initialize(metadata_t *meta)
{
	int r, c, p = meta->n - 2;
	while (!check_prime(p)) p++;
	int sh = p + 2 - meta->n;
	meta->pr = p;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p - 1; c++)
			elem = create_elem(r, c - sh, elem);
		meta->chains[r] = create_elem(r, p - 1 - sh, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((r + p - c + 1) % p)
				elem = create_elem((r + p - c) % p, c - sh, elem);
		meta->chains[meta->w + r] = create_elem(r, p - sh, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((r + c + 1) % p)
				elem = create_elem((r + c) % p, c - sh, elem);
		meta->chains[meta->w * 2 + r] = create_elem(r, p + 1 - sh, elem);
	}
	return 0;
}

static int xicode_initialize(metadata_t *meta)
{
	int i, r, c, p = meta->n - 1;
	while (!check_prime(p)) p++;
	int sh = p + 1 - meta->n;
	meta->pr = p;
	meta->w = p - 1;
	meta->chains = (element_t **) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < p - 1; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if (r + 1 != c && r + c != p - 1)
				elem = create_elem(r, c - sh, elem);
		meta->chains[r] = create_elem(r, p - sh, elem);
	}
	for (i = 0; i < p - 1; i++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++) {
			r = (p + i - c) % p;
			if (r != p - 1 && r + 1 != c && r + c != p - 1)
				elem = create_elem(r, c - sh, elem);
		}
		meta->chains[meta->w + i] = create_elem(i, i + 1 - sh, elem);
	}
	for (i = 0; i < p - 1; i++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++) {
			r = (c + i) % p;
			if (r != p - 1 && r + 1 != c && r + c != p - 1)
				elem = create_elem(r, c - sh, elem);
		}
		meta->chains[meta->w * 2 + i] = create_elem(i, p - 1 - i - sh, elem);
	}
	return 0;
}

static int pcode_initialize(metadata_t *meta)
{
	int i, j, r, c, p = meta->pr = meta->n + 1;
	if (!check_prime(p)) return -1;
	meta->w = meta->n / 2;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	int *rno = malloc(sizeof(int) * meta->n);
	memset(rno, 0, sizeof(int) * meta->n);
	for (i = 0; i < p - 1; i++)
		for (j = i + 1; j < p - 1; j++) {
			c = (i + j + 1) % p;
			if (c == p - 1) continue;
			r = ++rno[c];
			meta->chains[i] = create_elem(r, c, meta->chains[i]);
			meta->chains[j] = create_elem(r, c, meta->chains[j]);
		}
	free(rno);
	for (c = 0; c < p - 1; c++)
		meta->chains[c] = create_elem(0, c, meta->chains[c]);
	return 0;
}

static int code56_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p - 1; c++)
			if (r != c)
				elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, r, elem);
	}
	for (r = 0; r < meta->w; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p - 1; c++)
			if ((r + c + 2) % p)
				elem = create_elem((r + c + 1) % p, c, elem);
		meta->chains[meta->w + r] = create_elem(r, p - 1, elem);
	}
	return 0;
}

static int cyclic_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n + 1;
	if (!check_prime(p)) return -1;
	meta->w = meta->n / 2;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	int alpha = -1, beta = -1, i, j, k = 2;
	for (i = 2; i < p; i++) {
		int a = i;
		for (j = 2; a != 1 && j < p; j++) {
			a = a * i % p;
			if (j == k && a == 1)
				alpha = i;
			if (j == p - 1 && a == 1 && beta < 0)
				beta = i;
		}
	}
	int *perm = (int*) malloc(sizeof(int) * meta->n);
	for (i = 0; i < p - 1; i++) {
		int a = 1;
		for (j = 0; j < i; j++)
			a = a * beta % p;
		perm[(a + p - 1) % p] = i;
	}
	int ll = 1, rr = alpha, rowno = 1;
	for (i = 0; i < meta->w; i++) {
		if (ll < p - 1 && rr < p - 1) {
			for (c = 0; c < meta->n; c++) {
				int x = (perm[ll] + c) % meta->n;
				int y = (perm[rr] + c) % meta->n;
				meta->chains[x] = create_elem(rowno, c, meta->chains[x]);
				meta->chains[y] = create_elem(rowno, c, meta->chains[y]);
			}
			rowno += 1;
		}
		ll = ll * beta % p;
		rr = rr * beta % p;
	}
	free(perm);
	for (c = 0; c < p - 1; c++)
		meta->chains[c] = create_elem(0, c, meta->chains[c]);
	return 0;
}

static int hdd1_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n - 1;
	if (!check_prime(p)) return -1;
	meta->w = p - 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (r = 0; r < p - 2; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if (r + c != p - 1)
				elem = create_elem(r, c, elem);
		meta->chains[r] = create_elem(r, p - r- 1, elem);
	}
	for (c = 0; c < p; c++) {
		element_t *elem = NULL;
		for (r = 0; r < p - 2; r++)
			elem = create_elem(r, (p - 2 + c - r) % p, elem);
		meta->chains[p - 2 + c] = create_elem(p - 2, c, elem);
	}
	for (r = 0; r < p - 1; r++) {
		element_t *elem = NULL;
		for (c = 0; c < p; c++)
			if ((c - r + 1) % p)
				elem = create_elem((c + p - r) % p, c, elem);
		meta->chains[meta->w * 2 + r] = create_elem(r, p, elem);
	}
	return 0;
}

static int crs_initialize(metadata_t *meta)
{
	int r, c, p = meta->pr = meta->n;
	int m = meta->m, n = meta->n;
	meta->w = 3;
	while ((1 << meta->w) < n) meta->w++;
	int w = meta->w, k = n - m;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	cauchy_setup_tables(w);
	int *mat = cauchy_good_general_coding_matrix(k, m, w);
	for (r = 0; r < m * w; r++) {
		element_t *elem = NULL;
		int rr = r / w;
		int mr = r % w;
		for (c = 0; c < k * w; c++) {
			int cc = c / w;
			int mc = c % w;
			int v = mat[rr * k + cc], t;
			for (t = 0; t < mc; t++)
				v = cauchy_mult(v, 2);
			if (1 & (v >> mr))
				elem = create_elem(mc, cc, elem);
		}
		meta->chains[r] = create_elem(mr, rr + k, elem);
	}
	return 0;
}

static int raid5_initialize(metadata_t *meta)
{
	int c;
	meta->w = 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	for (c = 0; c < meta->n; c++)
		meta->chains[0] = create_elem(0, c, meta->chains[0]);
	return 0;
}

static int raid0_initialize(metadata_t *meta)
{
	meta->w = 1;
	meta->chains = (element_t**) malloc(meta->w * meta->m * sizeof(void*));
	memset(meta->chains, 0, meta->w * meta->m * sizeof(void*));
	return 0;
}

static int erasure_make_table(metadata_t *meta)
{
	int i, did = 0, pid = 0;
	element_t *elem;
	int nr_data = meta->w * meta->k;
	int nr_total = meta->w * meta->n;
	int nr_parity = meta->w * meta->m;
	if (nr_parity > 64) return -1;
	meta->loc_d = (element_t*) malloc(nr_data * sizeof(element_t));
	meta->map_p = (int**) malloc(nr_data * sizeof(void*));
	meta->page = (page_t*) malloc(nr_total * sizeof(page_t));
	int *parity = (int*) malloc(nr_total * sizeof(int));
	memset(parity, -1, nr_total * sizeof(int));
	for (i = 0; i < nr_parity; i++) {
		elem = meta->chains[i];
		parity[ID(elem->row, elem->col)] = pid++;
	}
	if (pid != nr_parity) return -1;
	for (i = 0; i < nr_total; i++)
		if (parity[i] < 0) {
			meta->map_p[did] = (int*) malloc(20 * sizeof(int));
			memset(meta->map_p[did], -1, 20 * sizeof(int));
			meta->loc_d[did].col = i % meta->n;
			meta->loc_d[did].row = i / meta->n;
			meta->loc_d[did].next = NULL;
			did += 1;
		}
	if (did != nr_data) return -1;
	int *depend = (int*) malloc(nr_parity * sizeof(int));
	int *queue  = (int*) malloc(nr_parity * sizeof(int));
	int *occur  = (int*) malloc(nr_total  * sizeof(int));
	memset(depend, 0, nr_parity * sizeof(int));
	memset(occur , 0, nr_total  * sizeof(int));
	int front = 0, back = 0;
	for (i = 0; i < nr_parity; i++) {
		for (elem = meta->chains[i]->next; elem != NULL; elem = elem->next)
			if (parity[ID(elem->row, elem->col)] != -1)
				depend[i] += 1;
		if (depend[i] == 0)
			queue[back++] = i;
	}
	while (front < back) {
		int ch = queue[front++];
		element_t *head = meta->chains[ch];
		for (elem = head->next; elem != NULL; elem = elem->next)
			occur[ID(elem->row, elem->col)] ^= 1;
		for (i = 0; i < nr_data; i++) {
			elem = meta->loc_d + i;
			int protect = occur[ID(elem->row, elem->col)];
			int j = 0, *par = meta->map_p[i];
			while (par[j] != -1) {
				elem = meta->chains[par[j++]];
				protect ^= occur[ID(elem->row, elem->col)];
			}
			if (protect) par[j] = ch;
		}
		for (elem = head->next; elem != NULL; elem = elem->next)
			occur[ID(elem->row, elem->col)] ^= 1;
		int currid = ID(head->row, head->col);
		for (i = 0; i < nr_parity; i++)
			for (elem = meta->chains[i]->next; elem != NULL; elem = elem->next)
				if (ID(elem->row, elem->col) == currid)
					if (!(--depend[i]))
						queue[back++] = i;
	}
	if (back != nr_parity) return -1;
	meta->map_p2 = (int**) malloc(sizeof(void*) * meta->n * meta->w);
	for (i = 0; i < nr_total; i++) {
		meta->map_p2[i] = (int*) malloc(sizeof(int) * 4);
		memset(meta->map_p2[i], 0, sizeof(int) * 4);
	}
	for (i = 0; i < nr_parity; i++)
		for (elem = meta->chains[i]; elem; elem = elem->next) {
			int uid = ID(elem->row, elem->col);
			meta->map_p2[uid][occur[uid]++] = i;
		}
	free(parity);
	free(depend);
	free(queue);
	free(occur);
	return 0;
}

static void create_code(int code, int level, const char *flag, const char *name, initializer_t func)
{
	specs[nr_codes].codeID = code;
	specs[nr_codes].level = level;
	specs[nr_codes].flag = flag;
	specs[nr_codes].name = name;
	specs[nr_codes].init = func;
	nr_codes++;
}

void erasure_initialize()
{
	create_code(CODE_RAID0, 0, "raid0", "RAID-0", raid0_initialize);
	create_code(CODE_RAID5, 1,"raid5", "RAID-5", raid5_initialize);
	create_code(CODE_RDP, 2, "rdp", "RDP", rdp_initialize);
	create_code(CODE_EVENODD, 2, "evenodd", "EVENODD", evenodd_initialize);
	create_code(CODE_HCODE, 2, "hcode", "H-code", hcode_initialize);
	create_code(CODE_XCODE, 2, "xcode", "X-code", xcode_initialize);
	create_code(CODE_LIBERATION, 2, "liberation", "Liberation", liberation_initialize);
	create_code(CODE_PCODE, 2, "pcode", "P-code", pcode_initialize);
	create_code(CODE_CYCLIC, 2, "cyclic", "Cyclic", cyclic_initialize);
	create_code(CODE_CODE56, 2, "code56", "Code.5-6", code56_initialize);
	create_code(CODE_STAR, 3, "star", "STAR", star_initialize);
	create_code(CODE_TRIPLE, 3, "triple", "Triple-Star", triple_initialize);
	create_code(CODE_HDD1, 3, "hdd1", "HDD1", hdd1_initialize);
	create_code(CODE_EXT_HCODE, 3, "exthcode", "Extended.H-code", ext_hcode_initialize);
	create_code(CODE_XICODE, 3, "xicode", "XI-code", xicode_initialize);
	create_code(CODE_CRS, 3, "crs", "Cauchy_RS", crs_initialize);
}

void erasure_code_init(metadata_t *meta, int codetype, int disks, int usize,
		erasure_complete_t comp, int checkmode)
{
	meta->codetype = codetype;
	meta->n = disks;
	meta->m = specs[code_id].level;
	meta->k = meta->n - meta->m;
	meta->usize = usize;
	meta->comp_fn = comp;
	for (code_id = 0; code_id < nr_codes; code_id++)
		if (meta->codetype == specs[code_id].codeID) {
			if (specs[code_id].init(meta) < 0) {
				fprintf(stderr, "invalid disk number using %s\n", specs[code_id].name);
				exit(0);
			}
			if (checkmode) {
				print_chains(meta);
				exit(0);
			}
			if (erasure_make_table(meta) < 0) {
				fprintf(stderr, "initialization failure\n");
				exit(0);
			}
			meta->sctlr = (stripe_ctlr_t*) malloc(sizeof(stripe_ctlr_t));
			sh_init(meta->sctlr, disks, meta->w, usize);
			sh_set_io_callbacks(meta->sctlr, erasure_maprequest, erasure_degraded, erasure_iocomplete);
			return;
		}
	fprintf(stderr, "unrecognized codetype in erasure_code_init()\n");
	exit(-1);
}

void erasure_handle_request(double time, metadata_t *meta, ioreq_t *req)
{
	req->out_reqs = 1;
	int end = req->blkno + req->bcount;
	int stripe_sz = meta->k * meta->w * meta->usize;
	int stripe_no = req->blkno / stripe_sz;
	int curr = stripe_no * stripe_sz;
	while (curr < end) {
		sub_ioreq_t *subreq = (sub_ioreq_t*) disksim_malloc(sb_idx);
		subreq->reqtype = REQ_TYPE_NORMAL;
		subreq->state = STATE_BEGIN;
		subreq->stripeno = stripe_no;
		subreq->blkno = max(0, req->blkno - curr);
		subreq->bcount = min(stripe_sz, end - curr) - subreq->blkno;
		subreq->flag = req->flag;
		subreq->reqctx = req;
		subreq->meta = (void*)meta;
		req->out_reqs += 1;
		sh_get_active_stripe(time, meta->sctlr, subreq);
		sh_add_history(meta->sctlr, subreq->stripeno);
		stripe_no += 1;
		curr += stripe_sz;
	}
	if ((--req->out_reqs) == 0)
		meta->comp_fn(time, req);
}

void erasure_maprequest(double time, sub_ioreq_t *subreq, stripe_head_t *sh)
{
	subreq->state += 1;
	subreq->out_reqs = 1;
	metadata_t *meta = (metadata_t*) subreq->meta;
	if (meta->codetype == CODE_RAID0)
		subreq->state += 1;
	int usize = meta->usize;
	int begin = subreq->blkno, end = begin + subreq->bcount;
	int flag = subreq->flag | DISKSIM_TIME_CRITICAL;
	int nr_total = meta->n * meta->w, pid;
	int did = subreq->blkno / meta->usize;
	int lo = did * usize; // current region = [lo, lo + usize]
	if (!(subreq->flag & DISKSIM_READ) && subreq->state == STATE_READ)
		flag |= DISKSIM_READ;
	meta->bitmap = 0;
	while (lo < end && lo + usize > begin) {
		int vb = max(0, begin - lo), ve = min(usize, end - lo);
		sh_request_t *shreq = (sh_request_t*) disksim_malloc(sh_idx);
		shreq->time = time;
		shreq->flag = flag;
		shreq->stripeno = subreq->stripeno;
		shreq->devno = meta->loc_d[did].col;
		shreq->blkno = meta->loc_d[did].row;
		shreq->reqctx = (void*) subreq;
		shreq->meta = (void*) meta->sctlr;
		subreq->out_reqs += 1;
		sh_request_arrive(time, meta->sctlr, sh, shreq);
		if (!(subreq->flag & DISKSIM_READ)) {
			int i = 0, *par = meta->map_p[did];
			while (par[i] != -1) {
				pid = par[i++];
				page_t *pg = meta->page + pid;
				meta->bitmap |= 1ll << pid;
			}
		}
		did += 1;
		lo += usize;
	}
	int nr_parity = meta->m * meta->w;
	if (!(subreq->flag & DISKSIM_READ))
		for (pid = 0; pid < nr_parity; pid++)
			if (1 & (meta->bitmap >> pid)) {
				page_t *pg = meta->page + pid;
				sh_request_t *shreq = (sh_request_t*) disksim_malloc(sh_idx);
				shreq->time = time;
				shreq->flag = flag;
				shreq->stripeno = subreq->stripeno;
				shreq->devno = meta->chains[pid]->col;
				shreq->blkno = meta->chains[pid]->row;
				shreq->reqctx = (void*) subreq;
				shreq->meta = (void*) meta->sctlr;
				subreq->out_reqs += 1;
				sh_request_arrive(time, meta->sctlr, sh, shreq);
			}
	erasure_iocomplete(time, subreq, sh);
}

void erasure_degraded(double time, sub_ioreq_t *subreq, stripe_head_t *sh, int fails, int *fd)
{
	subreq->state += 1;
	subreq->out_reqs = 1;
	metadata_t *meta = (metadata_t*) subreq->meta;
	int usize = meta->usize;
	int begin = subreq->blkno, end = begin + subreq->bcount;
	int flag = subreq->flag | DISKSIM_TIME_CRITICAL;
	int nr_total = meta->n * meta->w;
	int did = subreq->blkno / meta->usize;
	int lo = did * usize; // current region = [lo, lo + usize]
	if (!(subreq->flag & DISKSIM_READ) && subreq->state == STATE_READ)
		flag |= DISKSIM_READ;
	memset(meta->page, 0, sizeof(page_t) * nr_total);
	while (lo < end && lo + usize > begin) {
		int vb = max(0, begin - lo), ve = min(usize, end - lo);
		element_t *elem = meta->loc_d + did;
		int unit_id = ID(elem->row, elem->col);
		if (fd[meta->loc_d[did].col]) {
			if (subreq->flag & DISKSIM_READ) {
				for (elem = meta->chains[meta->map_p2[unit_id][0]]; elem; elem = elem->next) {
					int new_id = ID(elem->row, elem->col);
					if (new_id == unit_id) continue;
					page_t *pg = meta->page + new_id;
					pg->state = 1;
				}
			} // do not handle write!
		} else {
			page_t *pg = meta->page + unit_id;
			pg->state = 1;
		}
		if (!(subreq->flag & DISKSIM_READ)) {
			int i = 0, *par = meta->map_p[did];
			while (par[i] != -1) {
				int pid = par[i++];
				element_t *elem = meta->chains[pid];
				if (!fd[elem->col]) {
					int new_id = ID(elem->row, elem->col);
					page_t *pg = meta->page + new_id;
					pg->state = 1;
				}
			}
		}
		did += 1;
		lo += usize;
	}
	int new_id;
	for (new_id = 0; new_id < nr_total; new_id++) {
		page_t *pg = meta->page + new_id;
		if (!(pg->state)) continue;
		sh_request_t *shreq = (sh_request_t*) disksim_malloc(sh_idx);
		shreq->time = time;
		shreq->flag = flag;
		shreq->stripeno = subreq->stripeno;
		shreq->devno = new_id % meta->n;
		shreq->blkno = new_id / meta->n;
		shreq->reqctx = (void*) subreq;
		shreq->meta = (void*) meta->sctlr;
		subreq->out_reqs += 1;
		sh_request_arrive(time, meta->sctlr, sh, shreq);
	}
	erasure_iocomplete(time, subreq, sh);
}

void erasure_iocomplete(double time, sub_ioreq_t *subreq, stripe_head_t *sh)
{
	metadata_t *meta = (metadata_t*) subreq->meta;
	if ((--subreq->out_reqs) == 0) {
		// check state & reqflag
		if ((subreq->flag & DISKSIM_READ) || subreq->state == STATE_WRITE) {
			sh_release_stripe(time, meta->sctlr, subreq->stripeno);
			ioreq_t *req = (ioreq_t*) subreq->reqctx;
			if ((--req->out_reqs) == 0)
				meta->comp_fn(time, (ioreq_t*) subreq->reqctx);
			disksim_free(sb_idx, subreq);
		} else
			sh_redo_callback(time, meta->sctlr, subreq, sh);
	}
}
