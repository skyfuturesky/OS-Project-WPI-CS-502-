/*
 * yuteng.c
 *
 *  Created on: Oct 2, 2014
 *      Author: skyfuture
 */

#include             "yuteng.h"

INT32 my_print;
INT32 scheduler_print;


PCB * OSCreateProcess(void * starting_address, INT32 priority, char * name, INT32 *pid_ptr, INT32 *error_ptr)
{
	void * context;
	PCB *k;
	int i;

	if(priority < 0){
		*error_ptr = 1;
		return NULL;
	}
	if(Name_Exist(name) == 0){
		*error_ptr = 2;
		return NULL;
	}

	// up limit 20 processes in timer queue, ready queue, suspend queue
	if(process_num >  19  ){
		*error_ptr = 3;
		return NULL;
	}

	k = (PCB *)malloc(sizeof(PCB));

	CALL(Z502MakeContext(&(context), starting_address, USER_MODE));

	k->z502_context = context;
	k->PID = process_num;
	process_num++;

	k->Priority = priority;
	k->Ab_Time = 0;
	k->Time = 0;

	k->Name = (char*) calloc(MAX_NAME_LENGTH, 1);
	strcpy(k->Name, name);

	k->next = NULL;

	k->waiting_for_message_number=0;
	k->source_pid=0;
	k->receive_message_length=0;

	*pid_ptr=k->PID;

	k->page_tbl_length = 1024;

	// initialize page table
	// at the very beginning, each page is invalid
	for(i=0; i<k->page_tbl_length; i++)
	{
	   k->page_table[i] = 0x0000;
	}

	for(i=0; i<k->page_tbl_length; i++)
	{
	   k->shadow_page_table[i] = 0x0000;
	}

	// copy over to context
	((Z502CONTEXT *)k->z502_context)->page_table_ptr = k->page_table;
	((Z502CONTEXT *)k->z502_context)->page_table_len = 1024;


	k->disk_data = NULL;
	k->disk_id = 0;
	k->disk_sector=0;
	k->read_write=-1;
	k->shadow_record = 0;

	// push every PCB to ready queue including current PCB
	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
	AddPCBToReadyQ(k);
	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);

	if(my_print > 0) printReadyQ();
	if(my_print > 0) printTimerQ();

	*error_ptr = 0;
	return k;

}

void svc_sleep(INT32 sleep_time)
{

	INT32 current_time;
	INT32 temp;

	// calculate absolute time
	MEM_READ(Z502ClockStatus, &current_time);
	temp = current_time + sleep_time;

	current_PCB->Ab_Time = temp;
	current_PCB->Time = sleep_time;
	if (my_print > 0) printf("current time is %d, alarm time %d\n", current_time, temp);

	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
	RemovePCBFromReadyQByPID(current_PCB->PID);
	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);

	READ_MODIFY(TIMERQ_LOCK, 1, 1, &lock_result);
	AddPCBToTimerQ(current_PCB);
	READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);

	if(scheduler_print > 0)
	{
		SP_printQ(SP_ACTION_MODE, "SLEEP");
		scheduler_print--;
	}

	restart_timer();
	scheduler();
}

INT32 svc_get_time_of_the_day(void)
{

	INT32 current_time;
    MEM_READ(Z502ClockStatus, &current_time);

    return current_time;
}

// svc terminate
void svc_terminate_process(INT32 pid, INT32 * error_ptr)
{

	// kill all
	if(pid == -2) Z502Halt();

	// kill itself
	if(pid == -1)
	{
		READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
		RemovePCBFromReadyQByPID((INT32)current_PCB->PID);
		process_num--;
		READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
	}
	// kill others
	else
	{
		READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
		if (RemovePCBFromReadyQByPID(pid) != NULL) process_num--;
		else if (RemovePCBFromTimerQByPID(pid) != NULL) process_num--;
		else if (RemovePCBFromSuspendQueueByPID(pid) != NULL) process_num--;
		READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
	}

	if(scheduler_print > 0)
	{
		SP_printQ(SP_ACTION_MODE, "TERMINAT");
		scheduler_print--;
	}

	// check if no process exists after killing one process
	if(process_num==0) Z502Halt();

	*error_ptr = 0;

 	scheduler();

}

// get process id
INT32 svc_get_process_id(char * process_name, INT32 * pid_ptr, INT32 * error_ptr)
{
	// input string is empty
 	if(process_name[0] == '\0')
 	{
 		*pid_ptr = current_PCB->PID;
		*error_ptr = 0;
		return *pid_ptr;
	}

 	PCB * k;
 	k = timer_head;

 	while(k != NULL)
 	{
 		if(strcmp(k->Name,process_name) == 0)
 		{
 			*pid_ptr = k->PID;
 			*error_ptr = 0;
 			return *pid_ptr;
 		}
 		k = k->next;
 	}

 	k = ready_head;

 	while(k != NULL)
 	{
 		if(strcmp(k->Name,process_name) == 0)
 		{
 			*pid_ptr = k->PID;
 			*error_ptr = 0;
 			return *pid_ptr;
 		}
 		k = k->next;
 	}

 	k = suspend_head;

 	while(k != NULL)
	{
 		if(strcmp(k->Name,process_name) == 0)
 		{
 			*pid_ptr = k->PID;
 			*error_ptr = 0;
 			return *pid_ptr;
 		}
 		k = k->next;
 	}

 	int i;
 	for (i=0;i<8;i++)
 	{
 		READ_MODIFY(DISK_LOCK+i, 1, 1, &lock_result);
 		k = disk_waiting_list[i];
 		while (k != NULL)
 		{
 			if (strcmp(k->Name,process_name) == 0)
 			{
 				*pid_ptr = k->PID;
 				*error_ptr = 0;
 				READ_MODIFY(DISK_LOCK+i, 0, 1, &lock_result);
 				return *pid_ptr;;
 			}
 			k = k->next;
 		}
 		READ_MODIFY(DISK_LOCK+i, 0, 1, &lock_result);
 	}


 	*error_ptr = 1;

 	if(scheduler_print > 0)
 	{
 		SP_printQ(SP_ACTION_MODE, "GET_PID");
 		scheduler_print--;
 	}

 	// -1 is invalid process ID
 	return -1;

}


void svc_suspend_process(INT32 pid, INT32 * error_ptr)
{

	PCB * k;

    if(pid == -1) pid = current_PCB->PID;

	READ_MODIFY(TIMERQ_LOCK, 1, 1, &lock_result);
	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
	READ_MODIFY(SUSPENDQ_LOCK, 1, 1, &lock_result);

    k = GetPCBFromQ(pid);

    // pid not exists
    if(k == NULL)
    {
    	READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
    	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
    	READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
    	*error_ptr = 1;
    	return;
    }

    // pid already suspended, and target pid is not waiting for message
    if(k->PState==SUSPEND)
    {
    	READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
    	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
    	READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
        *error_ptr = 2;
        return;
    }

    // cannot suspend current PCB
    if (k->PID == current_PCB->PID)
    {
    	READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
    	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
    	READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
        *error_ptr = 3;
        return;
    }

    k->PState = SUSPEND;

    // if in ready queue, move to suspend queue
    if(ExistInReadyQ(pid) == 0)
    {
		RemovePCBFromReadyQByPID(pid);
		AddPCBToSuspendQueue(k);
    }

	*error_ptr = 0;

    READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
    READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
    READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);

    if(scheduler_print > 0)
    {
    	SP_printQ(SP_ACTION_MODE, "SUSPEND");
    	scheduler_print--;
    }

}


void svc_resume_process(INT32 pid, INT32 * error_ptr)
{
	PCB * k;

	READ_MODIFY(TIMERQ_LOCK, 1, 1, &lock_result);
	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
	READ_MODIFY(SUSPENDQ_LOCK, 1, 1, &lock_result);

	k = GetPCBFromQ(pid);

	// pid not found
	if(k == NULL)
	{
    	READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
    	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
    	READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
        *error_ptr = 1;
        return;
	}

	// pcb not suspended
	if(k->PState != SUSPEND)
	{
    	READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
    	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
    	READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
        *error_ptr = 2;
        return;
	}

	RemovePCBFromSuspendQueueByPID(pid);

	// in timer queue
	if(ExistInTimerQ(pid)==0) k->PState = TIMER;

	AddPCBToReadyQ(k);

	READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
	READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);

	if(my_print > 0) printf("address %p, current state of k %d\n",k,k->PState);

	if(scheduler_print > 0)
	{
		SP_printQ(SP_ACTION_MODE, "RESUME");
		scheduler_print--;
	}

	*error_ptr = 0;

}


void svc_change_priority(INT32 pid, INT32 priority, INT32 * error_ptr)
{
	PCB * k;

	// change itself
	if(pid == -1) k = current_PCB;
	else k = GetPCBFromQ(pid);

	if(k == NULL)
	{
		*error_ptr = 1;
		return;
	}

	// valid priority range from 0 to 99
	if( priority > 99 || priority < 0)
	{
		*error_ptr = 2;
		return;
	}

	k->Priority = priority;

	*error_ptr = 0;

	ReOrderReadyQ();

	if(scheduler_print > 0)
	{
		SP_printQ(SP_ACTION_MODE, "CHANGE_P");
		scheduler_print--;
	}

}


// check if there is a need to reset the timer or let the timer retain its current timing
void restart_timer(void)
{
	 INT32 current_time;

     INT32 sleep_time;
     INT32 temp;

	 READ_MODIFY(TIMERQ_LOCK, 1, 1, &lock_result);
	 if(timer_head == NULL)
	 {
		 READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
		 return;
	 }

	 READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);

	 MEM_READ(Z502ClockStatus, &current_time);

	 CleanUpTimerQ();

	 // after clean up, check whether timer queue is empty
	 READ_MODIFY(TIMERQ_LOCK, 1, 1, &lock_result);
	 if(timer_head == NULL)
	 {
		 READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
		 return;
	 }

	 temp = timer_head->Ab_Time;
	 READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);

	 sleep_time = temp - current_time;

	 // start the timer
	 MEM_WRITE(Z502TimerStart, &sleep_time);


}

// scheduler
void scheduler(void)
{

	 if (my_print > 0) printf("scheduling\n");

	 //timer queue and ready queue, suspend queue are NULL
	 if(process_num==0) Z502Halt();

	 READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);

	 // ready queue is NULL, should enter idle state, wait for interrupt
	 while(ready_head == NULL)
	 {
		 READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
		 // idle sometimes doesn't work, so put it in while loop
		 //Z502Idle();
		 // Z502Idle doesn't work for test2c for idling forever error
		 CALL(time_increase());
		 READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
	 }

	 // ready queue is not NULL
	 // reschedule current PCB from ready queue
	 current_PCB = ready_head;

	 READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);

	 // print information
	 if(scheduler_print>0)
	 {
		 SP_printQ(SP_ACTION_MODE, "SCHEDULE");
		 scheduler_print--;
	 }

	 //execute the new PCB
	 CALL(Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context)));
}

void timer_interrupt(void)
{
    CleanUpTimerQ();
    restart_timer();
}

void time_increase(void)
{
	int i;
	for (i=0;i<1000;i++){}
}

void time_increase1(void)
{
	int i;
	for (i=0;i<10000;i++){}
}

void svc_send_message(INT32 target_pid, char * contents, INT32 message_length, INT32 * error_ptr)
{

	// up limit 10 messages in total
	if (message_num > 9)
	{
		*error_ptr = 1;
		return;
	}

	// message length exceeds maximum allowed length
	if (message_length > 512)
	{
		*error_ptr = 2;
		return;
	}

	READ_MODIFY(TIMERQ_LOCK, 1, 1, &lock_result);
	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
	READ_MODIFY(SUSPENDQ_LOCK, 1, 1, &lock_result);

	// target pid doesn't exist
	if ((GetPCBFromQ(target_pid) == NULL) && (target_pid!=-1))
	{
		*error_ptr = 3;
		READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
		READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
		READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
		return;
	}

	READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
	READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);

	// create new message
	message * new_msg = malloc(sizeof(message));
	new_msg->target_pid = target_pid;
	// always send from current PCB
	new_msg->source_pid = current_PCB->PID;
	new_msg->send_length = message_length;

	// assign message PID
	new_msg->PID = message_num;
	message_num++;

	// message buffer size 512
	new_msg->contents = malloc(512*sizeof(char));

	strcpy(new_msg->contents,contents);

	READ_MODIFY(MESSAGEQ_LOCK, 1, 1, &lock_result);
	// put to message queue
	AddMessageToMessageQ(new_msg);
	READ_MODIFY(MESSAGEQ_LOCK, 0, 1, &lock_result);

	// broadcast message
    if(target_pid == -1)
    {
    	PCB * k = suspend_head;
    	while(k != NULL)
    	{
    		// it is waiting, and waiting for broadcast
   			if(k->source_pid == -1 && k->PState == WAITING_IN_SUSPEND) break;
   			k = k->next;
    	}

    	if(k != NULL)
    	{
    		// resume target PCB
    		READ_MODIFY(SUSPENDQ_LOCK, 1, 1, &lock_result);
    		RemovePCBFromSuspendQueueByPID(k->PID);
    		READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
    		READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
    		AddPCBToReadyQ(k);
    		READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
    	}

        *error_ptr=0;

        return;
    }

    // send to a target pid
    PCB * k;
    k = GetPCBFromQ(target_pid);

    // wake up PCB which is waiting for message in suspend queue
   	if ((ExistInSuspendQ(target_pid) == 0) && (k->PState == WAITING_IN_SUSPEND) && ((k->source_pid == current_PCB->PID) || (k->source_pid == -1)))
   	{
   		READ_MODIFY(SUSPENDQ_LOCK, 1, 1, &lock_result);
   		k = RemovePCBFromSuspendQueueByPID(target_pid);
   		READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
   		// ready to receive message
   		READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
   		AddPCBToReadyQ(k);
   		READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
   	    k->waiting_for_message_number++;
   	    *error_ptr = 0;
   	    if(scheduler_print>0)
   	    {
   	    	SP_printQ(SP_ACTION_MODE, "SEND");
   	    	scheduler_print--;
   	    }
   	    scheduler();
   	    return;
   	}

    k->waiting_for_message_number++;
    *error_ptr = 0;

    scheduler();
}


void svc_receive_message(INT32 source_pid, char * contents, INT32 receive_length, INT32 * actual_send_length, INT32 * message_sender_pid, INT32 * error_ptr)
{

	if(receive_length > 512)
	{
		*error_ptr = 1;
		return;
	}

	READ_MODIFY(TIMERQ_LOCK, 1, 1, &lock_result);
	READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
	READ_MODIFY(SUSPENDQ_LOCK, 1, 1, &lock_result);

	if((GetPCBFromQ(source_pid) == NULL) && (source_pid != -1))
	{
		*error_ptr = 2;
		READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
		READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
		READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
		return;
	}

	READ_MODIFY(TIMERQ_LOCK, 0, 1, &lock_result);
	READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
	READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);

	// there is no message ready for current PCB, so reschedule it
	if(current_PCB->waiting_for_message_number <= 0)
	{
		READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
		RemovePCBFromReadyQByPID(current_PCB->PID);
		READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
		// now current PCB is waiting for message in suspend queue
		current_PCB->PState = WAITING_IN_SUSPEND;
		current_PCB->source_pid = source_pid;
		current_PCB->receive_message_length = receive_length;
		READ_MODIFY(SUSPENDQ_LOCK, 1, 1, &lock_result);
		AddPCBToSuspendQueue(current_PCB);
		READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
		scheduler();
    }

	// receive from broadcast message
	// simply take from message queue
	if(source_pid == -1)
	{
		message * msg;
		msg = SearchMessage(-2,current_PCB->PID);

		// if no valid message exists, let the current PCB waiting for message
		if(msg == NULL)
		{
			READ_MODIFY(READYQ_LOCK, 1, 1, &lock_result);
			RemovePCBFromReadyQByPID(current_PCB->PID);
			READ_MODIFY(READYQ_LOCK, 0, 1, &lock_result);
			current_PCB->source_pid = source_pid;
			current_PCB->receive_message_length = receive_length;
			// now it's waiting for a message
			// since it didn't receive a message
			current_PCB->PState = WAITING_IN_SUSPEND;
			READ_MODIFY(SUSPENDQ_LOCK, 1, 1, &lock_result);
			AddPCBToSuspendQueue(current_PCB);
			READ_MODIFY(SUSPENDQ_LOCK, 0, 1, &lock_result);
			scheduler();
			return;
		}

		// message exists
		// too long

		if(msg->send_length > receive_length)
		{
			*error_ptr = 1;
			return;
		}

		// receive message
		strcpy(contents,msg->contents);
		current_PCB->source_pid = 0;

		// delete message from message queue
		READ_MODIFY(MESSAGEQ_LOCK, 1, 1, &lock_result);
		RemoveMessageFromMessageQByPID(msg->PID);
		READ_MODIFY(MESSAGEQ_LOCK, 0, 1, &lock_result);
		message_num--;

		// current PCB received the message
		if(msg->target_pid == current_PCB->PID) current_PCB->waiting_for_message_number--;

		*actual_send_length = msg->send_length;
		*message_sender_pid = msg->source_pid;
		*error_ptr = 0;

		if(scheduler_print>0) SP_printQ(SP_ACTION_MODE, "RECEIVE");
		scheduler();
		return;
	}

	// receive message from certain source_pid
	message * msg = SearchMessage(source_pid,current_PCB->PID);

	// receive length too short, message too long
	if(msg->send_length > receive_length)
	{
		*error_ptr = 1;
		return;
	}

	strcpy(contents,msg->contents);

	READ_MODIFY(MESSAGEQ_LOCK, 1, 1, &lock_result);
	RemoveMessageFromMessageQByPID(msg->PID);
	message_num--;
	READ_MODIFY(MESSAGEQ_LOCK, 0, 1, &lock_result);

	current_PCB->source_pid = 0;
	current_PCB->waiting_for_message_number--;

	*actual_send_length = msg->send_length;
	*message_sender_pid = msg->source_pid;
	*error_ptr = 0;

	if(scheduler_print>0)
	{
		SP_printQ(SP_ACTION_MODE, "RECEIVE");
		scheduler_print--;
	}
	scheduler();

}

