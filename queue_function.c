/*
 * queue_function.c
 *
 *  Created on: Sep 29, 2014
 *      Author: skyfuture
 */

#include 		"queue_function.h"

INT32  lock_result = 0;
INT32  process_num = 0;

PCB * ready_head;
PCB * timer_head;
PCB * suspend_head;
PCB * current_PCB;

PCB * disk_waiting_list[8];

// there is no need to free space when quitting the program
// no physical queue head and tail exists in the space when
// there is nothing in the queue
void InitializeQueues(void)
{
	ready_head = NULL;
	timer_head = NULL;
	current_PCB = NULL;
}

// check duplicated names
INT32 Name_Exist(char * process_name)
{

	if (process_name[0] == '\0') return 0;

	if((current_PCB != NULL)&&(strcmp(current_PCB->Name,process_name) == 0)) return 0;

	PCB * k;
	k = timer_head;

	while(k != NULL)
	{
		if(strcmp(k->Name,process_name) == 0) return 0;
		k = k->next;
	}

	k = ready_head;

	while(k != NULL)
	{
		if(strcmp(k->Name,process_name) == 0) return 0;
		k = k->next;
	}

	k = suspend_head;

	while(k != NULL)
	{
		if(strcmp(k->Name,process_name) == 0) return 0;
		k = k->next;
	}

	return 1;

}

// push one PCB into the ready queue
void AddPCBToReadyQ(PCB * pcb)
{
    pcb->PState = READY;

    PCB* k = ready_head;

    // ready queue is empty
	if(ready_head == NULL )
	{
		ready_head = pcb;
		return;
	}

	// insert before ready queue's first element
	if (ready_head->Priority > pcb->Priority)
	{
		pcb->next = ready_head;
		ready_head = pcb;
		return;
	}

	// insert in the ready queue
	while(k->next!=NULL)
	{
		if(pcb->Priority < k->next->Priority) break;
		k = k->next;
	}

	pcb->next = k->next;
	k->next = pcb;

}

// pop out one item from ready queue by its PID
PCB * RemovePCBFromReadyQByPID(INT32 pid)
{
	// ready queue is empty
	if(ready_head == NULL) return NULL;

	PCB* k = ready_head;

	if(k->PID == pid)
	{
		ready_head=k->next;
		k->next=NULL;
		return k;
	}

	while(k->next != NULL)
	{
		if(k->next->PID == pid)
		{
			PCB * l = k->next;
			k->next = k->next->next;
			l->next = NULL;
			return l;
		}
		k = k->next;
	}

	return NULL;
}

PCB * RemovePCBFromReadyQ(void)
{

	if(ready_head == NULL) return NULL;

	PCB * k= ready_head;

	ready_head = k->next;
	k->next = NULL;
	return k;
}

UINT32 NumOfProcessInReadyQ(void)
{

	UINT32 k;
	PCB *l;
	k = 0;

	l = timer_head;

	while(l->next->PID != -1)
	{
		k++;
		l = l->next;
	}

	return k;
}


// check whether current PID exists in ready queue
INT32 ExistInReadyQ(INT32 pid)
{
	if(ready_head == NULL) return 1;

	PCB * k =  ready_head;

	while(k != NULL)
	{
		if(k->PID == pid) return 0;
		k = k->next;
	}

	return 1;
}

// once one priority has been changed, ready queue has to be reordered
// assumption is only one PCB in the queue has been changed with priority
void ReOrderReadyQ(void)
{
	PCB * k;
	PCB * l;

	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);

	k = ready_head;

	if(k == NULL)
	{
		READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
		return;
	}

	l = k->next;

	while(l != NULL)
	{
		if(k->Priority > l->Priority) break;
		k = k->next;
		l = l->next;
	}

	if(l != NULL)
	{
		RemovePCBFromReadyQByPID(l->PID);
		AddPCBToReadyQ(l);
	}
	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
}

// push a PCB to timer queue
// according to its absolute time
void AddPCBToTimerQ(PCB * pcb)
{

	pcb->PState = TIMER;

	PCB * k = timer_head;

	// timer queue is empty
	if(timer_head == NULL)
	{
		timer_head = pcb;
		return;
	}

	// insert before ready queue's first element
	if (timer_head->Ab_Time > pcb->Ab_Time)
	{
		pcb->next = timer_head;
		timer_head = pcb;
		return;
	}

	// insert in the ready queue
	while(k->next!=NULL)
	{
		if(pcb->Ab_Time < k->next->Ab_Time) break;
		k = k->next;
	}

	pcb->next = k->next;
	k->next = pcb;

}


// pop out one item from timer queue by its PID
PCB * RemovePCBFromTimerQByPID(INT32 pid)
{
	// timer queue is empty
	if(timer_head == NULL) return NULL;

	PCB* k = timer_head;

	if(k->PID == pid)
	{
		timer_head=k->next;
		k->next=NULL;
		return k;
	}

	while(k->next != NULL)
	{
		if(k->next->PID == pid)
		{
			PCB * l = k->next;
			k->next = k->next->next;
			l->next = NULL;
			return l;
		}
		k = k->next;
	}

	return NULL;
}


// pop out the first element from the timer queue
// if current PCB's absolute time is larger than current time
// then return nothing
PCB * RemovePCBFromTimerQ(void)
{
	INT32 current_time;

    MEM_READ(Z502ClockStatus, &current_time);

	if(timer_head == NULL) return NULL;

	PCB * k= timer_head;

	// nothing is to be popped
	if(k->Ab_Time > current_time) return NULL;

	timer_head = k->next;
	k->next = NULL;
	return k;
}

// remove all the items that has not been popped out, earlier than current time
// it's necessary since the simulator is unstable !!!!!
void CleanUpTimerQ(void)
{

 	READ_MODIFY(TIMERQ_LOCK, 1, 1, &lock_result);
 	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);

	if(timer_head == NULL)
	{
	 	 READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
	 	 READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
	 	 return;
	}

	PCB * k;
	k = RemovePCBFromTimerQ();

	while(k != NULL)
	{
		// push to suspend queue of ready queue
		if(k->PState == SUSPEND) AddPCBToSuspendQueue(k);
		else					 AddPCBToReadyQ(k);
		k = RemovePCBFromTimerQ();
	}

 	 READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
 	 READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);

}

UINT32 NumOfProcessInTimerQ(void)
{

	UINT32 k;
	PCB *l;
	k = 0;

	l = timer_head;

	while(l->next->PID != -1)
	{
		k++;
		l = l->next;
	}

	return k;
}

// check whether current PID exists in timer queue
INT32 ExistInTimerQ(INT32 pid)
{
	if(timer_head == NULL) return 1;

	PCB * k = timer_head;

	while(k != NULL)
	{
		if(k->PID == pid) return 0;
		k = k->next;
	}

	return 1;
}


void AddPCBToSuspendQueue(PCB * pcb)
{

	if(suspend_head==NULL){
		suspend_head=pcb;
		return;
	}

	PCB * k = suspend_head;
	while(k->next != NULL) k = k->next;
	k->next = pcb;

}


PCB * RemovePCBFromSuspendQueueByPID(INT32 pid)
{
	if(suspend_head == NULL) return NULL;

	PCB * p= suspend_head;


	if(p->PID == pid)
	{
		suspend_head=p->next;
		p->next=NULL;
		return p;
	}

	while(p->next != NULL)
	{
		if(p->next->PID == pid)
		{
			PCB * q = p->next;
			p->next = p->next->next;
			q->next = NULL;
			return q;
		}
		p = p->next;
	}

	return NULL;

}

// pop out the first element from the suspend queue
PCB * RemovePCBFromSuspendQ(void)
{
	if(suspend_head == NULL) return NULL;

	PCB * k= suspend_head;

	suspend_head = k->next;
	k->next = NULL;
	return k;
}

// check whether current PID exists in suspend queue
INT32 ExistInSuspendQ(INT32 pid)
{
	if(suspend_head == NULL) return 1;

	PCB * k = suspend_head;

	while(k != NULL)
	{
		if(k->PID == pid) return 0;
		k = k->next;
	}

	return 1;
}

// according to pcb's process id, get corresponding PCB
PCB* GetPCBFromQ(INT32 pid)
{
	if ((current_PCB != NULL) && (current_PCB->PID == pid)) return current_PCB;

	PCB * k;

	k = timer_head;

	while(k != NULL)
	{
		if(k->PID == pid) return k;
		k = k->next;
	}


	k = ready_head;

	while(k != NULL)
	{
		if(k->PID == pid) return k;
		k = k->next;
	}

	k = suspend_head;

	while(k != NULL)
	{
		if(k->PID == pid) return k;
		k = k->next;
	}

	return NULL;

}

void printReadyQ(void)
{

	printf("ready queue\n");

	if(ready_head == NULL)
	{
		printf("ready queue is empty.\n");
	    return;
	}

	PCB * k = ready_head;

	while(k != NULL)
	{
			printf("process address %p, process id is %d, process name %s\n",k,k->PID,k->Name);
			k = k->next;
	}

	printf("done printing ready queue\n");
 }


void printTimerQ(void)
{

	printf("timer queue\n");

	if(ready_head == NULL)
	{
		printf("timer queue is empty.\n");
	    return;
	}

	PCB * k = ready_head;

	while(k != NULL)
	{
			printf("process address %p, process id is %d, process name %s, time is %d\n",k,k->PID,k->Name, k->Ab_Time);
			k = k->next;
	}

	printf("done printing timer queue\n");
 }


void printSuspendQ(void)
{
	printf("suspend queue\n");

	if(ready_head == NULL)
	{
		printf("suspend queue is empty.\n");
	    return;
	}

	PCB * k = ready_head;

	while(k != NULL)
	{
			printf("process address %p, process id is %d, process name %s\n",k,k->PID,k->Name);
			k = k->next;
	}

	printf("done printing suspend queue\n");
 }


void SP_printQ(int mode, char * mode_name)
{

	PCB  * k;

	// print out running and target processes
	SP_setup(SP_RUNNING_MODE, current_PCB->PID);

	// only 8 chars are allowed here
	SP_setup_action(mode, mode_name);

	INT32 current_time;

	MEM_READ(Z502ClockStatus, &current_time);

	SP_setup(SP_TIME_MODE, current_time);

	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
	//print out ready queue
	k = ready_head;
	while(k != NULL)
	{
		if (k->PID != current_PCB->PID) SP_setup(SP_READY_MODE,k->PID);
		k = k->next;
	}
	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);

	READ_MODIFY(TIMERQ_LOCK, 1, 1, &lock_result);
	// print out timer queue
	k = timer_head;
	while(k != NULL)
	{
		SP_setup(SP_TIMER_SUSPENDED_MODE,k->PID);
		k = k->next;
	}
	READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);

	READ_MODIFY(SUSPENDQ_LOCK, 1, 1, &lock_result);

	// print out suspend queue
	k = suspend_head;
	while(k != NULL)
	{
		SP_setup(SP_PROCESS_SUSPENDED_MODE,k->PID);
		k = k->next;
	}
	READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);

	// print out disk queue
	int i;
	for (i=0;i<8;i++)
	{
		READ_MODIFY(DISK_LOCK+i-1, 1, 1, &lock_result);
		k = disk_waiting_list[i];
		while(k != NULL)
		{
			SP_setup(SP_DISK_SUSPENDED_MODE,k->PID);
			k = k->next;
		}
		READ_MODIFY(DISK_LOCK+i-1, 0, 1, &lock_result);
	}

	// SP_do_output
	SP_print_line();

}


void lock_func(INT32 VirtualAddress, INT32 NewLockValue, INT32 Suspend, INT32 *SuccessfulAction)
{
	READ_MODIFY(&VirtualAddress,NewLockValue,Suspend,SuccessfulAction);
	printf("%p, lock result is %d\n",VirtualAddress, *SuccessfulAction);

}
