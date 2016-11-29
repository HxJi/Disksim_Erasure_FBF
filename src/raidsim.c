/*
 * raidsim.c
 *
 *  Created on: Mar 26, 2014
 *      Author: zyz915
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "disksim_erasure.h"
#include "disksim_event_queue.h"
#include "disksim_global.h"
#include "disksim_interface.h"
#include "disksim_malloc.h"
#include "disksim_rand48.h"
#include "disksim_req.h"
#include "disksim_timer.h"

#define EVENT_STOP_SIM			0
#define EVENT_TRACE_FETCH		1
#define EVENT_TRACE_RECON		2
#define EVENT_TRACE_MAPREQ		3
#define EVENT_DISK_FAILURE		4
#define EVENT_IO_COMPLETE		5
#define EVENT_IO_INTERNAL		6

struct disksim_interface *interface;

static event_queue_t *eventq;
static metadata_t *meta; // erasure code metadata
static double currtime = 0;
static double scale = 1;
static int reqno = 0;
static int disks = 12;
static int unit = 16; // size in KB
static int checkmode = 0;
static int fail = -1;
static long long limit = 0; // maximum operations
static const char *code = "rdp";
static const char *result = "";
static const char *parfile = "../valid/16disks.parv";
static const char *outfile = "t.outv";
static const char *inpfile = "";
static int notrace = 0;

extern int dr_idx, en_idx, rq_idx;

int usage(const char *main)
{
	printf("usage: %s [options]:\n", main);
	printf("  options are:\n");
	printf("\t-n, --num     [number]\t number of disks\n");
	printf("\t-u, --unit    [number]\t stripe unit size (KB)\n");
	printf("\t-s, --stop    [number]\t stop simulation after some seconds\n");
	printf("\t-i, --input   [string]\t input trace file\n");
	printf("\t-o, --output  [string]\t DiskSim output file\n");
	printf("\t-c, --code    [string]\t erasure code [rdp, evenodd, hcode, xcode, liberation, star, shcode]\n");
	printf("\t-p, --parv    [string]\t parv file\n");
	printf("\t-a, --append  [string]\t append results to file\n");
	printf("\t-f, --failure [string]\t set failed disks separated by ','\n");
	printf("\t-h, --help            \t help information\n");
	return 0;
}

void trace_add_next(FILE *f)
{
	static char line[201];
	if (fgets(line, 200, f) == NULL) {
		notrace = 1;
		return;
	}
	ioreq_t *req = create_ioreq();
	memset(req, 0, sizeof(ioreq_t));
	if (sscanf(line, "%lf%*d%ld%d%d", &req->time, &req->blkno, &req->bcount, &req->flag) != 4) {
		fprintf(stderr, "Wrong number of arguments for I/O trace event type\n");
		fprintf(stderr, "line: %s\n", line);
		exit(-1);
	}
	req->time *= scale;
	req->reqno = ++reqno; // auto increment ID
	event_queue_add(eventq, create_event(req->time, EVENT_TRACE_MAPREQ, req));
	event_queue_add(eventq, create_event(req->time, EVENT_TRACE_FETCH, f));
}

static double sum = 0;
static int numreqs = 0;

double avg_response_time()
{
	return sum / numreqs;
}

void ioreq_complete_callback(double time, ioreq_t *req)
{
	sum += (time - req->time);
	numreqs += 1;
	disksim_free(rq_idx, req);
	if (notrace && numreqs == reqno)
		event_queue_add(eventq, create_event(time + 1e-3, EVENT_STOP_SIM, NULL));
}

void iface_complete_callback(double time, struct disksim_request *dr, void *ctx)
{
	event_queue_add(eventq, create_event(time, EVENT_IO_COMPLETE, (void*)dr));
}

void iface_schedule_callback(disksim_interface_callback_t fn, double time, void *ctx)
{
	event_queue_add(eventq, create_event(time, EVENT_IO_INTERNAL, ctx));
}

void iface_descheduled_callback(double time, void *ctx)
{
}


void print_stat(FILE *f) {
/*
	if (f == NULL) return;
	fprintf(f, "===================================================\n");
	fprintf(f, "Trace File = %s\n", inpfile);
	fprintf(f, "Disks = %d\n", disks);
	fprintf(f, "Prime Number = %d\n", meta->pr);
	fprintf(f, "Unit Size = %d\n", unit);
	fprintf(f, "Avg. Response Time = %f ms\n", avg_response_time());
	fprintf(f, "Code = %s\n", get_code_name(get_code_id(code)));
	fprintf(f, "Total Simulation Time = %.3f s\n", currtime / 1000.0);
	fprintf(f, "Experiment Duration = %.3f s\n", timer_microsecond(TIMER_GLOBAL) / 1000.0);
*/

	//fprintf(f, "===================================================\n");
	char* start;
	char* end;

	
	if (f == NULL) return;
	//fprintf(f, "inpfile=%s\n", inpfile);
	
	//print code
	start=strstr(inpfile, "trace/");
	start=start+6;
	end=strstr(inpfile,"_p");
	while(start!=end){
	    fprintf(f, "%c", *start);
	    start++;
	}
	fprintf(f, ",");

	//print prime
	start=strstr(inpfile, "p=");
	start=start+2;
	end=strstr(start,"_");
	while(start!=end){
	    fprintf(f, "%c", *start);
	    start++;
	}
	fprintf(f, ",");

	//print error
	start=strstr(inpfile, "error=");
	start=start+6;
	end=strstr(start,"_");
	while(start!=end){
	    fprintf(f, "%c", *start);
	    start++;
	}
	fprintf(f, ",");

	//print stripe
	start=strstr(inpfile, "stripe=");
	start=start+7;
	end=strstr(start,"_");
	while(start!=end){
	    fprintf(f, "%c", *start);
	    start++;
	}
	fprintf(f, ",");

	//print cache size
	start=strstr(inpfile, "cache=");

	if (start==NULL)
		fprintf(f, "0,-,");

	else{
		start=start+6;
		end=strstr(start,"_");
		while(start!=end){
	    		fprintf(f, "%c", *start);
	    		start++;
		}
		fprintf(f, ",");

		//print cache method
		start=end+1;
		end=strstr(start,".trace");
		while(start!=end){
	    		fprintf(f, "%c", *start);
	    		start++;
		}
		fprintf(f, ",");

	}
	fprintf(f, "%.3f\n", currtime / 1000.0);
}

int main(int argc, char **argv)
{
	int i, p = 1;
	while (p < argc) {
		const char *flag = argv[p++];
		if (!strcmp(flag, "-h") || !strcmp(flag, "--help"))
			return usage(argv[0]);
		if (!strcmp(flag, "--check")) {
			checkmode = 1;
			continue;
		}
		if (p == argc) {
			fprintf(stderr, "require arguments after \"%s\" flag.\n", flag);
			exit(-1);
		}
		const char *argu = argv[p++];
		if (!strcmp(flag, "-p") || !strcmp(flag, "--parv"))
			parfile = argu;
		else if (!strcmp(flag, "-i") || !strcmp(flag, "--input"))
			inpfile = argu;
		else if (!strcmp(flag, "-o") || !strcmp(flag, "--output"))
			outfile = argu;
		else if (!strcmp(flag, "-n") || !strcmp(flag, "--num"))
			disks = atoi(argu);
		else if (!strcmp(flag, "-u") || !strcmp(flag, "--unit"))
			unit = atoi(argu);
        else if (!strcmp(flag, "-l") || !strcmp(flag, "--limit"))
        	limit = atol(argu);
        else if (!strcmp(flag, "-a") || !strcmp(flag, "--append"))
        	result = argu;
        else if (!strcmp(flag, "-f") || !strcmp(flag, "--fail"))
        	fail = atoi(argu);
        else if (!strcmp(flag, "-c") || !strcmp(flag, "--code"))
        	code = argu;
        else if (!strcmp(flag, "--scale"))
        	scale = atof(argu);
        else {
        	fprintf(stderr, "unknown flag: %s\n", flag);
        	exit(0);
        }
	}

	fprintf(stdout, "inpfile=%s\n", inpfile);

	interface = disksim_interface_initialize(
			parfile, outfile, iface_complete_callback,
			iface_schedule_callback, iface_descheduled_callback,
			(void*) interface, 0, 0);

	DISKSIM_srand48(1);

	malloc_initialize();
	erasure_initialize();

	eventq = (event_queue_t*) malloc(sizeof(event_queue_t));
	event_queue_init(eventq);

	meta = (metadata_t*) malloc(sizeof(metadata_t));
	erasure_code_init(meta, get_code_id(code), disks, unit * 2,
			ioreq_complete_callback, checkmode);

	FILE *inp = fopen(inpfile, "r");
	if (inp != NULL)
		event_queue_add(eventq, create_event(0, EVENT_TRACE_FETCH, inp));
	if (fail >= 0)
		event_queue_add(eventq, create_event(-1e-3, EVENT_DISK_FAILURE, (void*)fail));

	timer_start(TIMER_GLOBAL);

	int stopsim = 0;
	double checkpoint = 0, last_internal = 0;
	while (!stopsim) {
		event_node_t *node = event_queue_pop(eventq);
		if (node == NULL)
			break; // stop simulation
		currtime = node->time;
		//printf("time = %f, type = %d\n", node->time, node->type);
		switch (node->type) {
		case EVENT_STOP_SIM:
			stopsim = 1;
			break;
		case EVENT_TRACE_FETCH:
			//printf("time = %f, type = EVENT_TRACE_FETCH\n", node->time);
			trace_add_next((FILE*)node->ctx);
			break;
		case EVENT_TRACE_MAPREQ:
			//printf("time = %f, type = EVENT_TRACE_ITEM\n", node->time);
			erasure_handle_request(node->time, meta, (ioreq_t*)node->ctx);
			break;
		case EVENT_DISK_FAILURE:
			sh_set_disk_failure(node->time, meta->sctlr, (int)node->ctx);
			break;
		case EVENT_IO_INTERNAL:
			//printf("time = %f, type = EVENT_IO_INTERNAL\n", node->time);
			if (last_internal != node->time)
				disksim_interface_internal_event(interface, last_internal = node->time, node->ctx);
			break;
		case EVENT_IO_COMPLETE:
			//printf("time = %f, type = EVENT_IO_COMPLETE\n", node->time);
			sh_request_complete(node->time, (struct disksim_request*)node->ctx);
			disksim_free(dr_idx, node->ctx);
			break;
		default:
			printf("time = %f, type = INVALID\n", node->time);
			exit(-1);
		}
		disksim_free(en_idx, node);
		if ((--limit) == 0) break;
		if (currtime > checkpoint) {
			putchar('*');
			fflush(stdout);
			checkpoint += 60000;
		}
	}
	printf("\n");

	disksim_interface_shutdown(interface, currtime);
	timer_stop(TIMER_GLOBAL);

	//print_stat(stdout);
	//print_stat(fopen(result, "a"));

	//print_stat(stdout);
	print_stat(fopen(result, "a"));
	return 0;
}

