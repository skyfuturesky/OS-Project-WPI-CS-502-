/*
 * memory_function.c
 *
 *  Created on: Nov 12, 2014
 *      Author: skyfuture
 */

#include "memory_function.h"

INT32 memory_print;

int frame_num_index = 0;

// return the appropriate physical frame number
INT32 memory_mapping(INT32 logical_address)
{

	if (memory_print > 0)
	{
		printMemory();
		memory_print--;
	}

	// page number is invalid
	if (logical_address >= 1024) return -1;
	if (logical_address < 0) return -1;

	int page_content;
	int current_frame_number;

	page_content = current_PCB->page_table[logical_address];

	// page is valid
	if ((page_content & PTBL_VALID_BIT) == 1) return page_content & 0x3F;

	// page is invalid, need to validate the page, assign it to a physical frame
	if(((page_content & PTBL_VALID_BIT) == 0) && (frame_num_index < 64))
	{
		UINT16 temp;

		temp = 0x8000 | frame_num_index;
		current_PCB->page_table[logical_address] = temp;

		temp = current_PCB->PID<<10 | logical_address;
		frame_table[frame_num_index] = temp;

		AddPageToPageQ(logical_address, current_PCB, frame_num_index);

		current_frame_number = frame_num_index;
		frame_num_index++;

		return current_frame_number;
	}

	// otherwise, memory is already occupied
	// need to do page replacement
	PAGE * page;

	// FIFO algorithm
	page = RemovePageFromPageQ();

	// returns current valid frame number in physical memory
	current_frame_number = page_replacement(page->PID, page->logical_address, logical_address);

	AddPageToPageQ(logical_address, current_PCB, current_frame_number);

	if(scheduler_print > 0)
	{
		SP_printQ(SP_ACTION_MODE, "memory");
		scheduler_print--;
	}

	return current_frame_number;
}

// do page replacement, also it returns current valid frame number
int page_replacement(INT32 process_id, INT32 logical_address, INT32 logical_address_re)
{

	PCB * p;
	int  frame_num;
	int  page_content;

	p = GetPCBFromQ(process_id);

	page_content = p->page_table[logical_address];
	frame_num = page_content & 0x3F;

	// set page to invalid
	p->page_table[logical_address] = 0;

	// check data is already modified, write memory to disk
	if((page_content & 0x4000) == 1)
	{

		//taught by TA, disk ID corresponds to process ID
		INT32 disk_id;
		disk_id = p->PID+1;   //each process has its own disk to write the data

		READ_MODIFY(DISK_LOCK+disk_id-1, 1, 1, &lock_result);
		READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);

		current_PCB->disk_id = disk_id;
		current_PCB->disk_sector = p->shadow_record;
		current_PCB->read_write = 1;

		Z502WritePhysicalMemory(frame_num, current_PCB->disk_data);

		RemovePCBFromReadyQByPID(current_PCB->PID);

		// put current PCB to disk waiting list
		AddPCBToDiskQueue(current_PCB, current_PCB->disk_id);

		RemovePCBFromReadyQByPID(current_PCB->PID);

		AddPCBToDiskQueue(current_PCB, current_PCB->disk_id);

		READ_MODIFY(DISK_LOCK+disk_id-1, 0, 1, &lock_result);
		READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);

		// update shadow page table
		p->shadow_page_table[logical_address] = p->shadow_record;

	} else if((p->shadow_page_table[logical_address_re] & 0x8000) !=0)
	// shadow page table stores a valid page
	{

		INT32 disk_id;
		disk_id = p->PID+1;

		READ_MODIFY(DISK_LOCK+disk_id-1, 1, 1, &lock_result);
		READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);

		p->disk_id = disk_id;
		p->disk_sector = p->shadow_record;
		p->read_write = 0;

		Z502ReadPhysicalMemory(frame_num,p->disk_data);

		RemovePCBFromReadyQByPID(p->PID);

		// put current PCB to disk waiting list
		AddPCBToDiskQueue(current_PCB, p->disk_id);

		RemovePCBFromReadyQByPID(p->PID);

		AddPCBToDiskQueue(p, p->disk_id);

		READ_MODIFY(DISK_LOCK+disk_id-1, 0, 1, &lock_result);
		READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);

	}

	int temp;

	// update frame table
	temp = p->PID<<10 | logical_address_re;
	frame_table[frame_num] |= temp ;

	// update page table
	// set it to a valid page
	temp =0x8000 | frame_num;
	p->page_table[logical_address_re] |= temp;

	return frame_num;
}

// add one page at the end of the page queue
void AddPageToPageQ(int logical_address, PCB * pcb, int frame_number)
{
	READ_MODIFY(PAGEQ_LOCK, 1, 1, &lock_result);

	PAGE * k;

	PAGE * p = page_head;

	k = malloc(sizeof(PAGE));

	k->PID = pcb->PID;
	k->logical_address = logical_address;
	k->frame_num = frame_number;

	k->next = NULL;

	if(page_head == NULL)
	{
		page_head =  k;
		READ_MODIFY(PAGEQ_LOCK, 0, 1, &lock_result);
		return;
	}

	// find place to insert new page
	while (p->next != NULL) p = p->next;

	p->next = k;

	READ_MODIFY(PAGEQ_LOCK, 0, 1, &lock_result);

}

// remove one page from page queue
PAGE * RemovePageFromPageQ(void)
{
	READ_MODIFY(PAGEQ_LOCK, 1, 1, &lock_result);

	if (page_head == NULL)
	{
		READ_MODIFY(PAGEQ_LOCK, 0, 1, &lock_result);
		return NULL;
	}

	PAGE * p =  page_head;
	page_head = p->next;

	p->next = NULL;

	READ_MODIFY(PAGEQ_LOCK, 0, 1, &lock_result);
	return p;
}

// handle disk interrupt
void disk_interrupt(INT32 Disk_ID)
{

	// have to lock disk before set disk ID
	READ_MODIFY(DISK_LOCK+Disk_ID-1, 1, 1, &lock_result);

	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);

	int status;

	// disk operation done, so pop PCB from disk waiting list
	// then push it back to ready queue

	PCB * p;

	p = RemovePCBFromDiskQueue(Disk_ID);

    AddPCBToReadyQ(p);

    // if there is another process waiting for current disk, start it
    if (disk_waiting_list[Disk_ID-1] != NULL)
    {

    	p = disk_waiting_list[Disk_ID-1];

    	// set disk ID
    	MEM_WRITE(Z502DiskSetID, &p->disk_id);

		// set disk sector, then set disk buffer
		MEM_WRITE(Z502DiskSetSector, &p->disk_sector);
		MEM_WRITE(Z502DiskSetBuffer, p->disk_data);

		MEM_WRITE(Z502DiskSetAction, &p->read_write);

		// start disk operation
		status = 0;
		MEM_WRITE(Z502DiskStart, &status);

    }

    READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
    READ_MODIFY(DISK_LOCK+Disk_ID-1, 0, 1, &lock_result);

}

// add PCB to disk waiting list according to disk ID
void AddPCBToDiskQueue(PCB * pcb, INT32 Disk_ID)
{

	// empty queue
	if (disk_waiting_list[Disk_ID-1] == NULL)
	{
		disk_waiting_list[Disk_ID-1] = pcb;
		return;
	}

	PCB * p;

	p = disk_waiting_list[Disk_ID-1];

	// find the place to insert PCB
	while (p->next != NULL)
	{
		p=p->next;
	}

	pcb->next = NULL;
	p->next = pcb;

}

// remove PCB from disk waiting list according to disk ID
PCB * RemovePCBFromDiskQueue(int Disk_ID)
{

	// nothing in the disk waiting list for current disk
	if (disk_waiting_list[Disk_ID-1] == NULL) return NULL;

	PCB * p;

	p =  disk_waiting_list[Disk_ID-1];

	// take out the topmost item in current disk's waiting list
	disk_waiting_list[Disk_ID-1] = p->next;

	p->next = NULL;

	return p;
}


// disk write SVC call
void svc_disk_write(INT32 Disk_ID, INT32 sector, char * disk_data)
{
	//if(Disk_ID == 8) printf("--------------------------------\n");

	int status;

	current_PCB->disk_id = Disk_ID;
	current_PCB->disk_sector = sector;
	current_PCB->disk_data = disk_data;
	current_PCB->read_write = 1;

	// push into disk queue
	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
	READ_MODIFY(DISK_LOCK+Disk_ID-1, 1, 1, &lock_result);

	RemovePCBFromReadyQByPID(current_PCB->PID);

	// put current PCB to disk waiting list
	AddPCBToDiskQueue(current_PCB, Disk_ID);

	PCB * p;

	p = disk_waiting_list[Disk_ID-1];

	// set disk ID
	MEM_WRITE(Z502DiskSetID, &p->disk_id);
	// read disk status
	MEM_READ(Z502DiskStatus, &status);

	if (status == DEVICE_FREE)
	{
		// set disk write or read
		MEM_WRITE(Z502DiskSetAction, &p->read_write);

		// set disk sector, then set disk buffer
		MEM_WRITE(Z502DiskSetSector, &p->disk_sector);
		MEM_WRITE(Z502DiskSetBuffer, p->disk_data);
		
		status = 0;
		// start disk operation
		MEM_WRITE(Z502DiskStart, &status);

		//if (p->disk_id == 8) printf("write----------------------------------\n");
	}

	READ_MODIFY(DISK_LOCK+Disk_ID-1, 0, 1, &lock_result);
	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);

	if(scheduler_print > 0)
	{
		SP_printQ(SP_ACTION_MODE, "DISK_WRITE");
		scheduler_print--;
	}

	// reschedule
	scheduler();

}

// disk read SVC call
void svc_disk_read(INT32 Disk_ID, INT32 sector, char * disk_data)
{
	//if(Disk_ID == 8) printf("--------------------------------\n");
	int status;

	current_PCB->disk_sector = sector;
	current_PCB->disk_data = disk_data;
	current_PCB->read_write = 0;
	current_PCB->disk_id = Disk_ID;

	// push into disk queue
	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
	READ_MODIFY(DISK_LOCK+Disk_ID-1, 1, 1, &lock_result);

	RemovePCBFromReadyQByPID(current_PCB->PID);

	// put current PCB to disk waiting list
	AddPCBToDiskQueue(current_PCB, Disk_ID);

	PCB * p;

	p = disk_waiting_list[Disk_ID-1];

	// set disk ID
	MEM_WRITE(Z502DiskSetID, &p->disk_id);
	// read disk status
	MEM_READ(Z502DiskStatus, &status);

	// device is free
	if (status == DEVICE_FREE)
	{
		// set disk read or write
		MEM_WRITE(Z502DiskSetAction, &p->read_write);

		// set disk sector, then set disk buffer
		MEM_WRITE(Z502DiskSetSector, &p->disk_sector);
		MEM_WRITE(Z502DiskSetBuffer, p->disk_data);
		
		status = 0;
		// start disk operation
		MEM_WRITE(Z502DiskStart, &status);
		//if (p->disk_id == 8) printf("read----------------------------------\n");
	}

	READ_MODIFY(DISK_LOCK+Disk_ID-1, 0, 1, &lock_result);
	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);

	if(scheduler_print > 0)
	{
		SP_printQ(SP_ACTION_MODE, "DISK_READ");
		scheduler_print--;
	}

	// reschedule
	scheduler();

}

// print memory
void printMemory()
{
	int i;
	int page_content;
	int pid;
	int logical_address;
	int status;
	PCB * pcb;

	// for each physical page
	for(i=0;i<64;i++)
	{
		// take
		page_content = frame_table[i];

		logical_address = page_content & 0xFFF;

		// get process id
		pid = page_content >> 12;

		// get the target PCB
		pcb = GetPCBFromQ(pid);

		page_content = pcb->page_table[logical_address];

		// current state of the page
		status = (page_content & 0xE000)>>13;

		// frame number, process id and logical address using current frame
		// frame's status
		MP_setup(i, pid, logical_address, status);
	}

	// output
	MP_print_line();
}

void printDiskQueue(void)
{
	int i;

	printf("printing disk queue\n");

	for (i=0;i<8;i++)
	{
		PCB * p;

		int k;
		k = i+1;
		// set disk ID
		MEM_WRITE(Z502DiskSetID, &k);

		//read disk status
		int status;
		MEM_READ(Z502DiskStatus, &status);

		p = disk_waiting_list[i];
		printf("disk id %d, status is %d\n",i+1,status);
		while (p != NULL)
		{
			printf("disk ID = %d, process ID = %d, PCB status = %d\n", i+1, p->PID, p->PState);
			p = p->next;
		}
		//READ_MODIFY(DISK_LOCK+i, 0, 1, &lock_result);
	}

	printf("done printing disk queue\n");

}


