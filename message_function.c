/*
 * message_function.c
 *
 *  Created on: Oct 7, 2014
 *      Author: skyfuture
 */

#include "message_function.h"

UINT32 message_num = 0;

void AddMessageToMessageQ(message *msg)
{
	// empty queue
	if(message_head == NULL)
	{
		message_head = msg;
		return;
	}

	message * k = message_head;

	while(k->next != NULL) k = k->next;
	k->next = msg;
	msg->next = NULL;

}

message * RemovePCBFromMessageQ(void)
{
	if(message_head == NULL) return NULL;

	message * k= message_head;

	message_head = k->next;
	k->next = NULL;
	return k;

}

message * RemoveMessageFromMessageQByPID(INT32 pid)
{
	// message queue is empty
	if(message_head == NULL) return NULL;

	message * k = message_head;

	if(k->PID == pid)
	{
		message_head = k->next;
		k->next = NULL;
		return k;
	}

	while(k->next != NULL)
	{
		if(k->next->PID == pid)
		{
			message * l = k->next;
			k->next = k->next->next;
			l->next = NULL;
			return l;
		}
		k = k->next;
	}

	return NULL;
}

// source pid -2, search only for target pid
message * SearchMessage(INT32 source_pid, INT32 target_pid)
{

	message * k;
	k = message_head;

	if(source_pid == -2)
	{
		while(k != NULL)
		{
			if((k->target_pid == target_pid) || (k->target_pid == -1)) return k;
			k = k->next;
		}
		return NULL;
	}

	while(k != NULL)
	{
		// find target message
		if((k->target_pid == target_pid) && (k->source_pid == source_pid)) return k;
		k = k->next;
	}

	return NULL;
}


