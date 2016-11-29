/*
 * disksim_timer.c
 *
 *  Created on: Apr 21, 2014
 *      Author: zyz915
 */

#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>

#define NUM_TIMERS		32

static long long total_milli[NUM_TIMERS] = {0};
static struct timeval start[NUM_TIMERS], stop[NUM_TIMERS];

void timer_start(int index)
{
	gettimeofday(start + index, NULL);
}

void timer_stop(int index)
{
	gettimeofday(stop + index, NULL);
	total_milli[index] += (stop[index].tv_sec - start[index].tv_sec) * 1000000ll
			+ (stop[index].tv_usec - start[index].tv_usec);
}

void timer_reset(int index)
{
	total_milli[index] = 0;
}

void timer_reset_all()
{
	memset(total_milli, 0, sizeof(long long) * NUM_TIMERS);
}

long long timer_millisecond(int index)
{
	return total_milli[index];
}

long long timer_microsecond(int index)
{
	return total_milli[index] / 1000;
}
