/*
 * disksim_timer.h
 *
 *  Created on: Apr 21, 2014
 *      Author: zyz915
 */

#ifndef DISKSIM_TIMER_H_
#define DISKSIM_TIMER_H_

#define TIMER_GLOBAL	0
#define TIMER_DISKSIM	1

//int  timer_index();
void timer_start(int index);
void timer_stop(int index);
void timer_reset(int index);
void timer_reset_all();
long long timer_millisecond(int index);
long long timer_microsecond(int index);

#endif /* DISKSIM_TIMER_H_ */
