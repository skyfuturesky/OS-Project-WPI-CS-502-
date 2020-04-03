/*
 * message_function.h
 *
 *  Created on: Oct 7, 2014
 *      Author: skyfuture
 */

#ifndef MESSAGE_FUNCTION_H_
#define MESSAGE_FUNCTION_H_

#include "queue_function.h"

struct message
{
	int   PID;		// record message's ID
	int	  target_pid;
	int   source_pid;
	int   send_length;
	int   receive_length;
	char *  contents;			// msg_buffer
	struct message *next;
};

typedef struct message message;

message * message_head;

extern UINT32 message_num;

// message queue operations
void AddMessageToMessageQ(message * );
message * RemovePCBFromMessageQ(void);
message * RemoveMessageFromMessageQByPID(INT32);
message * SearchMessage(INT32, INT32);

#endif /* MESSAGE_FUNCTION_H_ */
