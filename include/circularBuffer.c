#include"rtool.h"

/* Prototypes in header ^^^ */

/* adds data to next available buffer position. returns that position */
int AddToBuffer(char *p, struct node *temp)
{
	/* flag for availability of current buffer slot */
	int ready = 0;

	/* changes buffer index to first available acked position */
	while (ready == 0) {

		/* Node to get info on current buffer slot of seq */
		struct node *ptr;
		ptr = (struct node *)malloc(sizeof(struct node));
		ptr->next = NULL;
		ptr = findNode(temp, end);

		if (ptr != NULL) {
			if (ptr->ack == 1) {
				// current buffer slot is ready
				ready = 1;
			} else {
				// increment buffer slot to check in next iteration
				end = (end + MSS) % CBUFFERSIZE;
			}
		}
		else {
			// buffer slot ready
			ready = 1;
		}
	}

	/* buffer slot being written to */
	int slot = end;

	/* copy data to slot */
    cBuffer[end] = malloc(MSS);
	bcopy(p, cBuffer[end], MSS);

	/* increment slot for next call */
    end = (end + MSS) % CBUFFERSIZE;

	/* for queue functionality */
    if (active <= CBUFFERSIZE)
    {
       active = active + MSS;
    } else {
        start = (start + MSS) % CBUFFERSIZE;
    }

	return slot;
}

/* similiar to above function but omits checking auxList for availability for efficenancy purposes */
int AddToBufferForServer(char *p)
{
	/* buffer slot being written to */
	int slot = end;

	/* copy data to slot */
    cBuffer[end] = malloc(MSS);
	bcopy(p, cBuffer[end], MSS);

	/* increment position for next call */
    end = (end + MSS) % CBUFFERSIZE;

	/* for queue functionality */
    if (active <= CBUFFERSIZE)
    {
       active = active + MSS;
    } else {
        start = (start + MSS) % CBUFFERSIZE;
    }

	return slot;
}

/* Function to get from buffer if used as a queue */
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

/* function to get data from buffer by specifying an index 0-63000 */
char * GetFromBufferByIndex(int index)
{
	char *p;

	/* copy data to p to return */
	p = cBuffer[index];

	return p;
}

/* returns true if all slots in buffer have been acked. Checks by searching auxList */
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

/* return start index */
int getStart() {
	return start;
}

/* return end index */
int getEnd() {
	return end;
}

/* Prints buffer for trouble shooting */
void displayBuffer() {
	int i = 0;
	printf("\nSTATUS OF BUFFER\nFront[%d]->", start);
	for (i = 0; i < CBUFFERSIZE; i = (i + MSS)) {
		printf("Index: %d, Contents: %s\n", i, cBuffer[i]);
	}
	printf("<-[%d]Rear", end);

}
