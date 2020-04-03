/*
 * yuteng.h
 *
 *  Created on: Oct 2, 2014
 *      Author: skyfuture
 */

#ifndef YUTENG_H_
#define YUTENG_H_

#include             "message_function.h"

extern INT32 my_print;
extern INT32 scheduler_print;

PCB * OSCreateProcess(void *, INT32, char *, INT32 *, INT32 *);

// SVC calls
void svc_sleep(INT32);
INT32 svc_get_time_of_the_day(void);
void svc_terminate_process(INT32, INT32 *);
INT32 svc_get_process_id(char *, INT32 *, INT32 *);
void svc_suspend_process(INT32, INT32 *);
void svc_resume_process(INT32, INT32 *);
void svc_change_priority(INT32, INT32, INT32 *);

// important functions
void restart_timer(void);
void scheduler(void);
void timer_interrupt(void);

// assistant functions
void time_increase(void);
void time_increase1(void);

// SVC calls: send/receive
void svc_send_message(INT32, char *, INT32, INT32 *);
void svc_receive_message(INT32, char* , INT32 , INT32 * , INT32 *, INT32 *);


#endif /* YUTENG_H_ */
