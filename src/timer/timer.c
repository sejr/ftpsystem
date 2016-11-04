/******************************************************************************/
/*  timer.c                                                                   */
/*  -------                                                                   */
/*  The timer allows us to limit the amount of time a packet can be in transit*/
/*  which subsequently allows us to handle timeouts, retransmission, etc.     */
/******************************************************************************/

/* HEADERS AND DEFINITIONS ****************************************************/

#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#define WAIT_TIME 1
#define LISTENING_PORT 9090
#define ETA "164.107.113.23"

#define COLOR_ERROR "\e[1m\x1b[31m"
#define COLOR_DEBUG "\e[1m\x1b[34m"
#define COLOR_RESET "\x1b[0m"

typedef struct timer_node {
	double time;
	int port;
	int seq_num;
	struct timer_node* next;
	struct timer_node* prev;
} TIMER_NODE_T;

/* Function prototypes ********************************************************/

void delete_node(int seq_num);
void add_node(double timeout, int seq_num, int port);
int insert_node(TIMER_NODE_T** head, TIMER_NODE_T* node);
int remove_node(int del_val, TIMER_NODE_T** head);
void print_full_list(TIMER_NODE_T* head);
TIMER_NODE_T* create_node(double time, int seq_num, int port);
void destroy_node(TIMER_NODE_T* node);
TIMER_NODE_T* head = NULL;

/* BEGIN timer.c **************************************************************/

// Destroys a timer node!
void destroy_node(TIMER_NODE_T* node) {
	free(node);
}

// New node with given time, sequence number, and port
TIMER_NODE_T* create_node(double time, int seq_num, int port) {
	TIMER_NODE_T* node = (TIMER_NODE_T*)malloc(sizeof(TIMER_NODE_T));
	if (NULL == node) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s\n",
                "ERROR: Failed to allocate memory for a new timer node");
	} else {
		node->next = NULL;
		node->prev = NULL;
		node->time = time;
		node->seq_num = seq_num;
		node->port = port;
	}
	return node;
}

// Print out entire linked list
void print_full_list(TIMER_NODE_T* list) {
	if (NULL == list) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s\n",
                "DELTA TIMER IS EMPTY");
	} else {
		while(NULL != list) {
			fprintf(stdout,
                    COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s%.2f%s%d%s\n",
                    "TIME @ ", list->time, " SEQ # ",
				list->seq_num,"");
			list = list->next;
		}
	}
}

// Insert node in given position
int insert_node(TIMER_NODE_T** head, TIMER_NODE_T* node) {
	//If the list is empty
	if (NULL == *head) {
		*head = node;
		return 1;
	}

	TIMER_NODE_T* curr_node = *head;
	TIMER_NODE_T* prev_node = NULL;

	while (NULL != curr_node && node->time > curr_node->time) {
		node->time -= curr_node->time;
		prev_node = curr_node;
		curr_node = curr_node->next;
	}

	//Insert at the end of the list
	if (NULL == curr_node) {
		prev_node->next = node;
		node->prev = prev_node;
		return 1;
	}

	//Insert as a new head node
	if (NULL == prev_node) {
		curr_node->prev = node;
		node->next = curr_node;
		*head = node;
		if (NULL != node->next) {
			node->next->time -= node->time;
		}
		return 1;
	}

	//Insert between two nodes
	prev_node->next = node;
	curr_node->prev = node;
	node->prev = prev_node;
	node->next = curr_node;
	if (NULL != node->next) {
		node->next->time -= node->time;
	}
	return 1;
}

// Remove node with particular sequence number
int remove_node(int del_val, TIMER_NODE_T** head) {
	//The list is empty
	if (NULL == *head) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
            "%s\n", "CAN'T DELETE; LIST EMPTY");
		return 0;
	}
	TIMER_NODE_T* trash = NULL;
	//The node to be removed is the head node
	if ((*head)->seq_num == del_val) {
		trash = *head;
		*head = (*head)->next;
		//Just in case we have removed the only node in the list
		if (NULL != *head) {
			(*head)->prev = NULL;
		}
		if (NULL != (*head)) {
			(*head)->time += trash->time;
		}
		destroy_node(trash);
		return 1;
	}

	TIMER_NODE_T* tracker = *head;
	while (NULL != tracker && tracker->seq_num != del_val) {
		tracker = tracker->next;
	}

	// Node not found
	if (NULL == tracker) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
        "%s%d%s\n", "CAN'T DELETE NODE W/ SEQ # ", del_val,
			". NODE NOT FOUND");
		return 0;
	}

	trash = tracker;
	(tracker->prev)->next = tracker->next;
	if (NULL != tracker->next) {
		(tracker->next)->prev = tracker->prev;
	}
	if (NULL != tracker->next) {
		tracker->next->time += tracker->time;
	}
	destroy_node(trash);
	return 1;
}



void add_node(double timeout, int seq_num, int port) {
	TIMER_NODE_T* node = create_node(timeout, seq_num, port);
	int check = insert_node(&head, node);

	if (check == 0) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] "
        COLOR_RESET "%s%.2f%s%d\n", "ERROR ADDING NODE "
			"timeout: ", timeout, " and seq_num number: ", seq_num);
	} else {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
        "%s%.2f%s%d%s\n", "NODE EXPIRING AT ", timeout, ", SEQ # "
			"", seq_num, " ADDED SUCCESSFULLY");
	}

	fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
    "%s\n", "DELTA TIMER STATUS");
	print_full_list(head);
}



/* This function is meant as a wrapper around remove_node(). It contains
   additional print statements for work flow tracking */
void delete_node(int seq_num) {
	int check = remove_node(seq_num, &head);

	if (check == 0) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
        "%s%d%s\n", "ERROR DELETING NODE SEQ # "
			"", seq_num, ". SEE MESSAGE");
	} else {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
        "%s%d%s\n", "NODE @ SEQ # ", seq_num, " DELETED "
			"SUCCESSFULLY");
	}

	fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
    "%s\n", "DELTA TIMER STATUS");
	print_full_list(head);

}

int main(int argc, char *argv[]) {
	fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
    "Delta timer initialized successfully.\n");

	int sock;
	struct sockaddr_in sin_addr;
	struct sockaddr_in timeout_response;

	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    	perror("Error opening socket");
    	exit(1);
    }

    sin_addr.sin_family = AF_INET;
    sin_addr.sin_port = htons(LISTENING_PORT);
    sin_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(sin_addr.sin_zero), '\0', 8);

    if(bind(sock,(struct sockaddr *)&sin_addr,sizeof(struct sockaddr_in)) < 0) {
      	perror("Error binding stream socket");
      	exit(1);
    }

    // Timeout destination
	bzero((char *)&timeout_response, sizeof timeout_response);
	timeout_response.sin_family = AF_INET;
	timeout_response.sin_addr.s_addr = inet_addr(ETA);
    timeout_response.sin_port = htons(8908);
	memset(&(sin_addr.sin_zero), '\0', 8);

    // Packet to create new timer nodes
    typedef struct recvd {
    	int type;
    	int seq_num;
    	double time;
    	int port;
    } recv_msg_t;

    recv_msg_t recv_msg;
    bzero((char*)&recv_msg, sizeof(recv_msg));

	// Packet for node timeout
    typedef struct send {
    	int flag;
    	int seq_num;
        double time;
    } send_msg_t;

	send_msg_t send_msg;
    bzero((char*)&send_msg, sizeof(send_msg));
    fd_set read_set;

    // Necessary variables
    struct timespec start_time, end_time, global_start_time;
	struct timeval wait_timer;
	wait_timer.tv_sec = 0;
	wait_timer.tv_usec = 50000;


	clock_gettime(CLOCK_MONOTONIC, &global_start_time);
	for(;;) {
		FD_ZERO(&read_set);
    	FD_SET(sock, &read_set);
		clock_gettime(CLOCK_MONOTONIC, &start_time);
		if (select(FD_SETSIZE, &read_set, NULL, NULL, &wait_timer) < 0) {
			fprintf(stderr, "%s\n", "There was an issue with select()");
			exit(1);
		}

		clock_gettime(CLOCK_MONOTONIC, &end_time);

		if(NULL != head) {
			double elapsed = ( end_time.tv_sec - start_time.tv_sec )
  					+ ( end_time.tv_nsec - start_time.tv_nsec )
  					/ 1E9;

  			head->time -= elapsed;
			while (NULL != head && head->time <= 0) {
				fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
                "%s%d%s\n", "SEQ # ", head->seq_num, " TIMED OUT!");

				//timeout_response.sin_port = htons(head->port);
				send_msg.flag = 2;
				send_msg.seq_num = head->seq_num;

				remove_node(head->seq_num, &head);
				fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
                    "%s\n", "NODE TIMED OUT");
				fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
                    "%s\n", "DELTA TIMER STATUS");
				print_full_list(head);


				//send message to client tcpd
				sendto(sock, (char*)&send_msg, sizeof send_msg,
					0, (struct sockaddr*)&timeout_response,
                    sizeof timeout_response);

			}
		}

        // Handle new requests
		if(FD_ISSET(sock, &read_set)) {
			if(recvfrom(sock,(char*)&recv_msg,sizeof(recv_msg),0,NULL,NULL)<0) {
				perror("There was an error receiving from the driver");
				exit(1);
			}
            // Node add requests
			if (recv_msg.type == 0) {
				fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
                    "%s%.2f%s%d\n", "NEW ADD REQ: ", recv_msg.time, ", ",
                    recv_msg.seq_num);
				add_node(recv_msg.time, recv_msg.seq_num, recv_msg.port);
			} else if (recv_msg.type == 1) {
                // Node remove requests
				fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET
                "%s%d\n", "NEW DELETE REQ: ", recv_msg.seq_num);
				delete_node(recv_msg.seq_num);
			}
			bzero((char*)&recv_msg, sizeof(recv_msg));
		}

	}

    // End timer process.
	close(sock);
	return 0;
}

/* END timer.c ****************************************************************/
