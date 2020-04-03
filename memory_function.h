/*
 * memory_function.h
 *
 *  Created on: Nov 12, 2014
 *      Author: skyfuture
 */

#ifndef MEMORY_FUNCTION_H_
#define MEMORY_FUNCTION_H_

#include "yuteng.h"

extern INT32 memory_print;

// totally 64 frames in my system
// bits 15-10 PID number, bits 9-0 virtual page number
UINT16 frame_table[64];

// keep track of current available physical frame number
extern int frame_num_index;

// record page mapping from PCB's page table to physical frame
// managed by OS
struct PAGE
{
	int PID;	// process ID
	int logical_address;	// logical address of page table in corresponding process
	int frame_num;			// physical frame number associated with the process's address
	struct PAGE * next;
};

typedef struct PAGE PAGE;

PAGE * page_head;

INT32 memory_mapping(INT32);

int page_replacement(INT32, INT32, INT32);

// page operation
void AddPageToPageQ(int, PCB *, int);
PAGE * RemovePageFromPageQ(void);

// disk interrupt
void disk_interrupt(INT32);
void finish_disk(long, long, char*, int);

// disk queue operation
void AddPCBToDiskQueue(PCB *, INT32);
PCB * RemovePCBFromDiskQueue(int);

// disk write/read SVC call
void svc_disk_write(INT32, INT32 , char *);
void svc_disk_read(INT32, INT32 , char *);

// print functions
void printMemory(void);
void printDiskQueue(void);

#endif /* MEMORY_FUNCTION_H_ */
