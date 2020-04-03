/*
 * queue_function.h
 *
 *  Created on: Sep 29, 2014
 *      Author: skyfuture
 */

#ifndef QUEUE_FUNCTION_H_
#define QUEUE_FUNCTION_H_

#include             "global.h"
#include             "string.h"
#include             "stdlib.h"
#include             "syscalls.h"
#include             "protos.h"
#include			 "z502.h"

extern INT32  lock_result;
extern INT32  process_num;

// lock addresses
#define			PCB_LOCK						 MEMORY_INTERLOCK_BASE
#define			TIMERQ_LOCK						 (MEMORY_INTERLOCK_BASE+1)
#define			READYQ_LOCK						 (MEMORY_INTERLOCK_BASE+2)
#define			SUSPENDQ_LOCK				     (MEMORY_INTERLOCK_BASE+3)
#define			MESSAGEQ_LOCK				     (MEMORY_INTERLOCK_BASE+4)
#define			PAGEQ_LOCK				    	 (MEMORY_INTERLOCK_BASE+5)
#define			DISK_LOCK					     (MEMORY_INTERLOCK_BASE+6)

// PCB name space
#define			MAX_NAME_LENGTH					 256

// PCB's current state
#define			SUSPEND               		     0
#define			TIMER	  		                 1
#define			READY                 	 	     2
#define			WAITING_IN_SUSPEND				 3
#define			DISK                 			 4 	 // wait for disk operation
#define			DISK_DONE                        5   // disk operation finished


// pcb itself has next, so both timer queue and ready queue will share
// similar PCB operation, which eases debugging
typedef struct pcb
{
	struct pcb  *next;
	void *z502_context;
	int PID;						// process ID
	int Priority;
	int Time;						// sleep time
	int Ab_Time;					// absolute time of the process to wake up
	char * Name;					// process name
	int PState;						// process's current state

	// message send/receive related
	int waiting_for_message_number;	// indicate how many messages PCB is waiting for
	int source_pid;					// indicate source PCB which sends the message
	int receive_message_length;

	// project 2
	UINT16  page_table[1024];               //1 valid bit, 1 modified bit, 1 referenced bit, 6 bit for frame number
	UINT16  shadow_page_table[1024];
	int page_tbl_length;

	// page replacement related
	int shadow_record;

	// disk related
	int disk_id;
	int disk_sector;
	char * disk_data;
	int read_write;

} PCB;

extern PCB * ready_head;
extern PCB * timer_head;
extern PCB * suspend_head;
extern PCB * current_PCB;

extern PCB * disk_waiting_list[8];

void InitializeQueues(void);

INT32 Name_Exist(char *);

// ready queue
void AddPCBToReadyQ(PCB *);
PCB * RemovePCBFromReadyQByPID(INT32);
PCB * RemovePCBFromReadyQ(void);
UINT32 NumOfProcessInReadyQ(void);
INT32 ExistInReadyQ(INT32);
void ReOrderReadyQ(void);

// timer queue
void AddPCBToTimerQ(PCB *);
PCB * RemovePCBFromTimerQByPID(INT32);
PCB * RemovePCBFromTimerQ(void);
void CleanUpTimerQ(void);
UINT32 NumOfProcessInTimerQ(void);
INT32 ExistInTimerQ(INT32);

// suspend queue
void AddPCBToSuspendQueue(PCB *);
PCB * RemovePCBFromSuspendQueueByPID(INT32);
PCB * RemovePCBFromSuspendQ(void);
INT32 ExistInSuspendQ(INT32);

PCB * GetPCBFromQ(INT32);

// printing method
void printReadyQ(void);
void printSuspendQ(void);
void printTimerQ(void);

// scheduler printer
void SP_printQ(int, char *);

// the lock function
void lock_func(INT32, INT32, INT32, INT32 *);

#endif /* QUEUE_FUNCTION_H_ */
