/************************************************************************

        This code forms the base of the operating system you will
        build.  It has only the barest rudiments of what you will
        eventually construct; yet it contains the interfaces that
        allow test.c and z502.c to be successfully built together.

        Revision History:
        1.0 August 1990
        1.1 December 1990: Portability attempted.
        1.3 July     1992: More Portability enhancements.
                           Add call to sample_code.
        1.4 December 1992: Limit (temporarily) printout in
                           interrupt handler.  More portability.
        2.0 January  2000: A number of small changes.
        2.1 May      2001: Bug fixes and clear STAT_VECTOR
        2.2 July     2002: Make code appropriate for undergrads.
                           Default program start is in test0.
        3.0 August   2004: Modified to support memory mapped IO
        3.1 August   2004: hardware interrupt runs on separate thread
        3.11 August  2004: Support for OS level locking
	4.0  July    2013: Major portions rewritten to support multiple threads
************************************************************************/


#include			 "memory_function.h"

extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;

// print for SVC/Fault/Interrupt
#define FULL_PRINT 10000
#define LIMIT_PRINT 10

INT32				base_print;

extern void          *TO_VECTOR [];

char                 *call_names[] = { "mem_read ", "mem_write",
                            "read_mod ", "get_time ", "sleep    ",
                            "get_pid  ", "create   ", "term_proc",
                            "suspend  ", "resume   ", "ch_prior ",
                            "send     ", "receive  ", "disk_read",
                            "disk_wrt ", "def_sh_ar" };


/************************************************************************
    INTERRUPT_HANDLER
        When the Z502 gets a hardware interrupt, it transfers control to
        this routine in the OS.
************************************************************************/
void    interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index = 0;

    device_id = 0;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );

    while (device_id != -1)
    {

    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

    // handle timer interrupt
    if (device_id == TIMER_INTERRUPT) timer_interrupt();

    // disk interrupt
	if ((device_id >= DISK_INTERRUPT) && (device_id <= (DISK_INTERRUPT + 7))) disk_interrupt(device_id-DISK_INTERRUPT+1);

    // Clear out this device - we're done with it
    MEM_WRITE(Z502InterruptClear, &Index );

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );

    }


}                                       /* End of interrupt_handler */


/************************************************************************
    FAULT_HANDLER
        The beginning of the OS502.  Used to receive hardware faults.
************************************************************************/

void    fault_handler( void )
    {
    INT32       device_id;
    INT32       status;
    INT32       Index = 0;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );

    if(base_print > 0)
    {
    	printf( "Fault_handler: Found vector type %d with value %d\n",
                        device_id, status );
    	base_print--;
    }

    // initially, current page is not linked to any physical frame, neither valid
	if(device_id == INVALID_MEMORY){
		// status is the page number
		// no such page exists, fault caused, halt the machine
		if (memory_mapping(status) == -1)
		{
			if (base_print > 0)
			{
				printf("no physical frame return\n");
				base_print--;
			}
			Z502Halt();
		}
	}

    if (device_id == PRIVILEGED_INSTRUCTION)
    {
    	if (base_print > 0)
    	{
    		printf("FALUT: privileged instruction\n");
    		base_print--;
    	}
    	Z502Halt();
    }

    // Clear out this device - we're done with it
    MEM_WRITE(Z502InterruptClear, &Index );
    //Z502Halt();

}                                       /* End of fault_handler */

/************************************************************************
    SVC
        The beginning of the OS502.  Used to receive software interrupts.
        All system calls come to this point in the code and are to be
        handled by the student written code here.
        The variable do_print is designed to print out the data for the
        incoming calls, but does so only for the first ten calls.  This
        allows the user to see what's happening, but doesn't overwhelm
        with the amount of data.
************************************************************************/

void    svc( SYSTEM_CALL_DATA *SystemCallData ) {
    short               call_type;
    short               i;

    // ********************* variables declarations ***********************

    INT32				current_time;
    INT32				pid;
    INT32				error = -1;
    INT32				temp;
    char 				process_name[MAX_NAME_LENGTH];
    INT32				priority;
    void *				function_address;
    char 				message_contents[512];
    INT32				actual_send_length;
    INT32				actual_source_id;
    INT32				address;
    INT32				Disk_ID;
    INT32				sector;
    char 				disk_data[32];

    // *******************************************************************

    call_type = (short)SystemCallData->SystemCallNumber;
    if (base_print > 0) {
        printf( "SVC handler: %s\n", call_names[call_type]);
        for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++ ){
        	 //Value = (long)*SystemCallData->Argument[i];
             printf( "Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
             (unsigned long )SystemCallData->Argument[i],
             (unsigned long )SystemCallData->Argument[i]);
        }
    base_print--;
    }

    switch(call_type){
     case SYSNUM_GET_TIME_OF_DAY:
    	CALL(current_time = svc_get_time_of_the_day());
    	*SystemCallData->Argument[0] = (long)current_time;
    	if (my_print > 0) printf("SVC: GET TIME: current time is %d\n", current_time);
     	break;

     //Sleep system call
     case SYSNUM_SLEEP:
    	temp = (INT32)SystemCallData->Argument[0];
    	if (my_print > 0) printf("SVC: SLEEP: sleep time is %d\n", temp);
     	CALL(svc_sleep(temp));
     	break;

     case SYSNUM_CREATE_PROCESS:
    	strcpy(process_name,(char *)SystemCallData->Argument[0]);
    	function_address = (void *)SystemCallData->Argument[1];
    	priority = (INT32)(SystemCallData->Argument[2]);
    	CALL(OSCreateProcess(function_address,priority,process_name,&pid,&error));
    	*SystemCallData->Argument[3] = pid;
    	*SystemCallData->Argument[4] = error;
    	if(my_print > 0)
    	{
    		if(error == 0) printf("New Process created: process name is %s, priority is %d, package ID is %d\n", process_name, priority, pid);
    		else if (error == 1) printf("Process Creation failed: illegal name\n");
    		else if (error == 2) printf("Process Creation failed: duplicated name\n");
    		else if (error == 3) printf("Process Creation failed: cannot create more than 20 processes\n");
    		else printf("Process Creation failed: unknown reason\n");
    	}
        break;

    //terminate system call
     case SYSNUM_TERMINATE_PROCESS:
    	temp = (INT32)SystemCallData->Argument[0];
    	if (my_print > 0) printf("SVC: TERMINATE PROCESS: %d\n",temp);
     	CALL(svc_terminate_process(temp,&error));
     	*SystemCallData->Argument[1] = error;
     	if (my_print > 0) printf("SVC: TERMINATE PROCESS: error %d\n", error);
     	break;

     case SYSNUM_GET_PROCESS_ID:
    	strcpy(process_name,(char *)SystemCallData->Argument[0]);
    	if (my_print > 0) printf("process name is %s\n", process_name);
     	CALL(svc_get_process_id(process_name,&pid,&error));
     	*SystemCallData->Argument[1] = pid;
     	*SystemCallData->Argument[2] = error;
     	break;

     case SYSNUM_SUSPEND_PROCESS:
    	pid = (INT32)SystemCallData->Argument[0];
    	if (my_print > 0) printf ("pid is %d\n", pid);
     	CALL(svc_suspend_process(pid,&error));
     	*SystemCallData->Argument[1] = error;
     	break;

     case SYSNUM_RESUME_PROCESS:
    	pid = (INT32)SystemCallData->Argument[0];
    	if (my_print > 0) printf ("pid is %d\n", pid);
     	CALL(svc_resume_process(pid,&error));
     	*SystemCallData->Argument[1] = error;
     	break;

     case SYSNUM_CHANGE_PRIORITY:
     	pid = (INT32)SystemCallData->Argument[0];
     	priority = (INT32)SystemCallData->Argument[1];
     	if (my_print > 0) printf ("pid is %d, priority to be changed is %d\n", pid, priority);
     	CALL(svc_change_priority(pid,priority,&error));
     	*SystemCallData->Argument[2] = error;
     	break;

     case SYSNUM_SEND_MESSAGE:
    	pid = (INT32)SystemCallData->Argument[0];
    	strcpy(message_contents,(char *)SystemCallData->Argument[1]);
    	temp = (INT32)SystemCallData->Argument[2];
     	CALL(svc_send_message(pid,message_contents,temp,&error));
     	*SystemCallData->Argument[3] = error;
     	break;

     case SYSNUM_RECEIVE_MESSAGE:
    	 // pid is td->source_pid
    	pid = (INT32)SystemCallData->Argument[0];
    	// temp is the receive length
    	temp = (INT32)SystemCallData->Argument[2];
     	svc_receive_message(pid,message_contents,temp,&actual_send_length,&actual_source_id,&error);
     	strcpy((char *)SystemCallData->Argument[1],message_contents);
     	*SystemCallData->Argument[3] = actual_send_length;
     	*SystemCallData->Argument[4] = actual_source_id;
     	*SystemCallData->Argument[5] = error;
     	if (my_print > 0) printf ("send length is %d, source id is %d, message is %s\n", actual_send_length, actual_source_id, message_contents);
     	break;

 	case SYSNUM_DISK_WRITE:
 		Disk_ID = (INT32)SystemCallData->Argument[0];
 		sector = (INT32)SystemCallData->Argument[1];
 		//strcpy(disk_data,(char *)SystemCallData->Argument[2]);
 		CALL(svc_disk_write(Disk_ID,sector,(char *)SystemCallData->Argument[2]));
 		break;

 	case SYSNUM_DISK_READ:
 		Disk_ID = (INT32)SystemCallData->Argument[0];
 		sector = (INT32)SystemCallData->Argument[1];
 		CALL(svc_disk_read(Disk_ID,sector,(char *)SystemCallData->Argument[2]));
 		//strcpy((char *)SystemCallData->Argument[2],disk_data);
 		break;

     default:
     	printf("ERROR! call_type not recognized! \n");
     	printf("Call_type is = %i\n", call_type);
    }
}                                               // End of svc



/************************************************************************
    osInit
        This is the first routine called after the simulation begins.  This
        is equivalent to boot code.  All the initial OS components can be
        defined and initialized here.
************************************************************************/

void    osInit( int argc, char *argv[]  ) {
    INT32               i;

    // -------------------- declarations ---------------------------------

    INT32				pid,error;

    // -------------------------------------------------------------------

    // -------------------- initializing procedure -----------------------
    InitializeQueues();

    // print or not
    my_print = 0;
    scheduler_print = 0;
    base_print = 0;
    memory_print = 0;

    // -------------------------------------------------------------------


    /* Demonstrates how calling arguments are passed thru to here       */

    printf( "Program called with %d arguments:", argc );
    for ( i = 0; i < argc; i++ )
        printf( " %s", argv[i] );
    printf( "\n" );
    printf( "Calling with argument 'sample' executes the sample program.\n" );

    /*          Setup so handlers will come to code in base.c           */

    TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR]   = (void *)interrupt_handler;
    TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR] = (void *)fault_handler;
    TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR]  = (void *)svc;

    /*  Determine if the switch was set, and if so go to demo routine.  */

	if (( argc > 1 ) && ( strcmp( argv[1], "sample" ) == 0 ) ) {
		CALL(current_PCB = OSCreateProcess((void *)sample_code, 10, "sample", &pid, &error));
		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
       }                  /* This routine should never return!!           */

    	if (( argc > 1 ) && ( strcmp( argv[1], "test0" ) == 0 ) ) {
    		base_print = FULL_PRINT;
			CALL(current_PCB = OSCreateProcess((void *)test0, 10, "test0", &pid, &error));
			Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
           }

    	if (( argc > 1 ) && ( strcmp( argv[1], "test1a" ) == 0 ) ) {
    		base_print = FULL_PRINT;
			CALL(current_PCB = OSCreateProcess((void *)test1a, 10, "test1a", &pid, &error));
			Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
           }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1b" ) == 0 ) ) {
        	base_print = FULL_PRINT;
    		CALL(current_PCB = OSCreateProcess((void *)test1b, 10, "test1b", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
           }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1c" ) == 0 ) ) {
        	base_print = LIMIT_PRINT;
        	scheduler_print = 1;
    		CALL(current_PCB = OSCreateProcess((void *)test1c, 10, "test1c", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
           }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1d" ) == 0 ) ) {
        	base_print = LIMIT_PRINT;
        	scheduler_print = 1;
    		CALL(current_PCB = OSCreateProcess((void *)test1d, 10, "test1d", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
           }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1e" ) == 0 ) ) {
        	base_print = FULL_PRINT;
    		CALL(current_PCB = OSCreateProcess((void *)test1e, 10, "test1e", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
           }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1f" ) == 0 ) ) {
        	base_print = LIMIT_PRINT;
        	scheduler_print = 1;
    		CALL(current_PCB = OSCreateProcess((void *)test1f, 10, "test1f", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1g" ) == 0 ) ) {
        	base_print = FULL_PRINT;
        	CALL(current_PCB = OSCreateProcess((void *)test1g, 10, "test1g", &pid, &error));
        	Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1h" ) == 0 ) ) {
        	base_print = LIMIT_PRINT;
        	scheduler_print = 1;
    		CALL(current_PCB = OSCreateProcess((void *)test1h, 10, "test1h", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1i" ) == 0 ) ) {
        	base_print = FULL_PRINT;
    		CALL(current_PCB = OSCreateProcess((void *)test1i, 10, "test1i", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1j" ) == 0 ) ) {
        	base_print = LIMIT_PRINT;
        	scheduler_print = 1;
    		CALL(current_PCB = OSCreateProcess((void *)test1j, 20, "test1j", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1k" ) == 0 ) ) {
        	base_print = FULL_PRINT;
    		CALL(current_PCB = OSCreateProcess((void *)test1k, 10, "test1k", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
       	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test1l" ) == 0 ) ) {
        	scheduler_print = 1;
    		CALL(current_PCB = OSCreateProcess((void *)test1l, 10, "test1l", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test2a" ) == 0 ) ) {
        	base_print = FULL_PRINT;
        	memory_print = FULL_PRINT;
    		CALL(current_PCB = OSCreateProcess((void *)test2a, 10, "test2a", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test2b" ) == 0 ) ) {
        	base_print = FULL_PRINT;
        	memory_print = FULL_PRINT;
    		CALL(current_PCB = OSCreateProcess((void *)test2b, 10, "test2b", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test2c" ) == 0 ) ) {
        	base_print = LIMIT_PRINT;
        	scheduler_print = FULL_PRINT;
    		CALL(current_PCB = OSCreateProcess((void *)test2c, 10, "test2c", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test2d" ) == 0 ) ) {
        	base_print = LIMIT_PRINT;
        	scheduler_print = FULL_PRINT;
        	//my_print = 1;
    		CALL(current_PCB = OSCreateProcess((void *)test2d, 10, "test2d", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test2e" ) == 0 ) ) {
        	base_print = LIMIT_PRINT;
        	scheduler_print = LIMIT_PRINT;
        	memory_print = LIMIT_PRINT;
    		CALL(current_PCB = OSCreateProcess((void *)test2e, 10, "test2e", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test2f" ) == 0 ) ) {
        	base_print = LIMIT_PRINT;
        	memory_print = LIMIT_PRINT;
    		CALL(current_PCB = OSCreateProcess((void *)test2f, 10, "test2f", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test2g" ) == 0 ) ) {
    		CALL(current_PCB = OSCreateProcess((void *)test2g, 10, "test2g", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        if (( argc > 1 ) && ( strcmp( argv[1], "test2h" ) == 0 ) ) {
    		CALL(current_PCB = OSCreateProcess((void *)test2h, 10, "test2h", &pid, &error));
    		Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(current_PCB->z502_context));
    	   }

        printf("wrong input, Usage: ./test1 test_name\n");
}                                               // End of osInit
