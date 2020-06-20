#ifndef LINKEDLIST_H
#define LINKEDLIST_H

struct listNode
{
	int *data;
	struct listNode *next;
};
void appendToList(struct listNode **head, int *data);
void delFirst(struct listNode **head);


#endif
