#include <stdio.h>
#include <stdlib.h>
#include "linkedlist.h"
void appendToList(struct listNode **head,int *data)
{
	struct listNode *new_node = (struct listNode *) malloc(sizeof(struct listNode));
	struct listNode *last=*head;
	new_node->data=data;
	new_node->next=NULL;
	if (*head==NULL)
	{
		*head=new_node;
		return;
	}
	while(last->next!=NULL)
		last=last->next;
	return;
}
void delFirst(struct listNode **head)
{
	struct listNode *tmp=*head;
	*head=(*head)->next;
	free(tmp);
}
