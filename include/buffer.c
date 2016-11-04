// buffer.c

/* INCLUDES *******************************************************************/

#include "rtool.h"

/* BEGIN buffer.c *************************************************************/

int AddToBuffer(char *p, struct node *temp)
{
        // is slot ready?
        int ready = 0;
        while (ready == 0) {
                struct node *ptr;
                ptr = (struct node *)malloc(sizeof(struct node));
                ptr->next = NULL;
                ptr = findNode(temp, end);

                if (ptr != NULL) {
                        if (ptr->ack == 1) {
                                // current buffer slot is ready
                                ready = 1;
                        } else {
                                end = (end + MSS) % CBUFFERSIZE;
                        }
                }
                else {
                        // buffer slot ready
                        ready = 1;
                }
        }

        int slot = end;
        cBuffer[end] = malloc(MSS);
        bcopy(p, cBuffer[end], MSS);
        end = (end + MSS) % CBUFFERSIZE;
        if (active <= CBUFFERSIZE)
        {
                active = active + MSS;
        } else {
                start = (start + MSS) % CBUFFERSIZE;
        }

        return slot;
}

int AddToBufferForServer(char *p)
{
        int slot = end;
        cBuffer[end] = malloc(MSS);
        bcopy(p, cBuffer[end], MSS);
        end = (end + MSS) % CBUFFERSIZE;
        if (active <= CBUFFERSIZE)
        {
                active = active + MSS;
        } else {
                start = (start + MSS) % CBUFFERSIZE;
        }

        return slot;
}

char * GetFromBuffer()
{
        char *p;

        if (!active) {
                return NULL;
        }

        p = cBuffer[start];
        start = (start + MSS) % CBUFFERSIZE;

        active = active - MSS;
        return p;
}

char * GetFromBufferByIndex(int index)
{
        char *p;

        /* copy data to p to return */
        p = cBuffer[index];

        return p;
}

int cBufferReady(struct node *temp) {
        int i = 0;
        int acked = 0;
        int notAcked = 0;
        for (i = 0; i < CBUFFERSIZE; i = (i + MSS)) {
                struct node *ptr;
                ptr = (struct node *)malloc(sizeof(struct node));
                ptr->next = NULL;
                ptr = findNode(temp, i);

                if (ptr->ack == 0) {
                        return 0;
                }
        }
        return 1;
}

int getStart() {
        return start;
}

int getEnd() {
        return end;
}

void displayBuffer() {
        int i = 0;
        printf("\nSTATUS OF BUFFER\nFront[%d]->", start);
        for (i = 0; i < CBUFFERSIZE; i = (i + MSS)) {
                printf("Index: %d, Contents: %s\n", i, cBuffer[i]);
        }
        printf("<-[%d]Rear", end);

}
