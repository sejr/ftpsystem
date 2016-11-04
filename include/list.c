// list.c

/* INCLUDES *******************************************************************/

#include "rtool.h"

/* METHODS ********************************************************************/

void insertNode(struct node *ptr,
    int start,
    int nextB,
    int pack,
    int bytes,
    int seq,
    int ack,
    struct timespec time)
{
        struct node* exist_node = (struct node *)malloc(sizeof(struct node));
        exist_node = findNode(ptr, start);

        if (exist_node == NULL) {

                while(ptr->next!=NULL)
                {
                        ptr = ptr->next;
                }
                ptr->next = (struct node *)malloc(sizeof(struct node));
                ptr = ptr->next;
                ptr->start = start;
                ptr->nextB = nextB;
                ptr->pack = pack;
                ptr->bytes = bytes;
                ptr->seq = seq;
                ptr->time = time;
                ptr->ack = ack;
                ptr->next = NULL;
        } else {
                exist_node->pack = pack;
                exist_node->bytes = bytes;
                exist_node->seq = seq;
                exist_node->time = time;
                exist_node->ack = ack;
        }
}

// Delete node with index
void deleteNode(struct node *ptr, int start)
{
        while(ptr->next!=NULL && (ptr->next)->start != start)
        {
                ptr = ptr->next;
        }
        if(ptr->next==NULL)
        {
                printf("Element %d is not present in the list\n",start);
                return;
        }
        struct node *temp;
        temp = ptr->next;
        ptr->next = temp->next;
        free(temp);
        return;
}

// Finds and returns node
struct node *findNode(struct node *ptr, int start)
{
        ptr = ptr->next;
        while(ptr!=NULL)
        {
                if(ptr->start == start)
                {
                        return ptr;
                }
                ptr = ptr->next;
        }
        return NULL;
}

// Finds node with seq #
struct node *findNodeBySeq(struct node *ptr, int seq)
{
        ptr = ptr->next;
        while (ptr != NULL)
        {
                if (ptr->seq == seq)
                {
                        return ptr;
                }
                ptr = ptr->next;
        }
        return NULL;
}

// Prints list
void printList(struct node *ptr)
{
        if(ptr==NULL)
        {
                return;
        }
        printf("Seq: %d Slot: %d Acked: %d\n",ptr->seq, ptr->start, ptr->ack);
        printList(ptr->next);
}
