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

/* This is the definition for each node within our delta timer linked list */
typedef struct timer_node {
	double time;
	int port;
	int seq_num;
	struct timer_node* next;
	struct timer_node* prev;
} timer_node_t;

/* These are the prototypes for the functions defined within this file */
void delete_node(int seq_num);
void add_node(double timeout, int seq_num, int port);
int insert_node(timer_node_t** head, timer_node_t* node);
int remove_node(int del_val, timer_node_t** head);
void print_full_list(timer_node_t* head);
timer_node_t* create_node(double time, int seq_num, int port);
void destroy_node(timer_node_t* node);



/* This pointer points to the head of the delta list */
timer_node_t* head = NULL;



/* This method deallocates the memory held by a node */
void destroy_node(timer_node_t* node) {
	free(node);
}



/* This method creates a new node for our linked list with the supplied criteria.
   It returns a pointer to the newly created node */
timer_node_t* create_node(double time, int seq_num, int port) {
	timer_node_t* node = (timer_node_t*)malloc(sizeof(timer_node_t));
	if (NULL == node) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s\n", "ERROR: Failed to allocate memory for a new timer node");
	} else {
		node->next = NULL;
		node->prev = NULL;
		node->time = time;
		node->seq_num = seq_num;
		node->port = port;
	}
	return node;
}



/* This method prints out the linked list in its entirety */
void print_full_list(timer_node_t* list) {
	if (NULL == list) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s\n", "The delta timer list is empty");
	} else {
		while(NULL != list) {
			fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s%.2f%s%d%s\n", "NODE:: time: ", list->time, " seq_num: ",
				list->seq_num," ---> ");
			list = list->next;
		}
	}
}



/* This function inserts the supplied node into the linked list. It returns 1
   for a successful insertion and 0 otherwise */
int insert_node(timer_node_t** head, timer_node_t* node) {
	//If the list is empty
	if (NULL == *head) {
		*head = node;
		return 1;
	}

	timer_node_t* curr_node = *head;
	timer_node_t* prev_node = NULL;

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



/* This function removes a node with a particular seq_num number from the list.
   It returns 1 for a successful removal and 0 otherwise */
int remove_node(int del_val, timer_node_t** head) {
	//The list is empty
	if (NULL == *head) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s\n", "ERROR: The list is empty. Cannot delete specified node.");
		return 0;
	}
	timer_node_t* trash = NULL;
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

	timer_node_t* tracker = *head;
	while (NULL != tracker && tracker->seq_num != del_val) {
		tracker = tracker->next;
	}
	//The node wasn't found
	if (NULL == tracker) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s%d%s\n", "ERROR: The node with seq_num number: ", del_val,
			" was never found. Failed to delete.");
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



/* This function is meant as a wrapper around inser_node(). It contains
   additional print statements for work flow tracking */
void add_node(double timeout, int seq_num, int port) {
	timer_node_t* node = create_node(timeout, seq_num, port);
	int check = insert_node(&head, node);

	if (check == 0) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s%.2f%s%d\n", "There was an issue adding the node with "
			"timeout: ", timeout, " and seq_num number: ", seq_num);
	} else {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s%.2f%s%d%s\n", "Node with timeout: ", timeout, " and seq_num "
			"number: ", seq_num, " added successfully");
	}

	fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s\n", "State of the delta timer:");
	print_full_list(head);
}



/* This function is meant as a wrapper around remove_node(). It contains
   additional print statements for work flow tracking */
void delete_node(int seq_num) {
	int check = remove_node(seq_num, &head);

	if (check == 0) {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s%d%s\n", "There was an issue deleting the node with "
			"seq_num number: ", seq_num, ". See above message.");
	} else {
		fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s%d%s\n", "Node with seq_num number: ", seq_num, " deleted "
			"successfully");
	}

	fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s\n", "State of the delta timer:");
	print_full_list(head);

}

int main(int argc, char *argv[]) {
	fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "Timer process booting up...\n\n");

	/* Create a socket for the timer to listen on */
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

    if(bind(sock, (struct sockaddr *)&sin_addr, sizeof(struct sockaddr_in)) < 0) {
      	perror("Error binding stream socket");
      	exit(1);
    }

    //Set up a partial address (no port) to send time out messages to
	bzero((char *)&timeout_response, sizeof timeout_response);
	timeout_response.sin_family = AF_INET;
	timeout_response.sin_addr.s_addr = inet_addr(ETA);
    timeout_response.sin_port = htons(8908);
	memset(&(sin_addr.sin_zero), '\0', 8);

    //The definition of the message received from the driver
    typedef struct recvd {
    	int type;
    	int seq_num;
    	double time;
    	int port;
    } recv_msg_t;

    recv_msg_t recv_msg;
    bzero((char*)&recv_msg, sizeof(recv_msg));

	//The definition of the message send to the tcpd client upon node timeout
    typedef struct send {
    	int flag;
    	int seq_num;
        double time;
    } send_msg_t;

	send_msg_t send_msg;
    bzero((char*)&send_msg, sizeof(send_msg));

    fd_set read_set;

    /* Create the variables that will be used for time keeping */
    struct timespec start_time, end_time, global_start_time;
	struct timeval wait_timer;
	wait_timer.tv_sec = 0;
	wait_timer.tv_usec = 50000;


	clock_gettime(CLOCK_MONOTONIC, &global_start_time);
	for(;;) {
		FD_ZERO(&read_set);
    	FD_SET(sock, &read_set);

    	/*TAKE TIME. START WAITING TO RECEIVE MESSAGE OR TIMEOUT*/
		clock_gettime(CLOCK_MONOTONIC, &start_time);
		if (select(FD_SETSIZE, &read_set, NULL, NULL, &wait_timer) < 0) {
			fprintf(stderr, "%s\n", "There was an issue with select()");
			exit(1);
		}

		/*WE HAVE EITHER RECEIVED A MESSAGE OR TIMED OUT. GET TIME*/
		clock_gettime(CLOCK_MONOTONIC, &end_time);
		/*printf("%s%f\n", "------------------CURRENT TIME----------------",
			 ((end_time.tv_sec - global_start_time.tv_sec )
  					+ ( end_time.tv_nsec - global_start_time.tv_nsec )
  					/ 1E9));*/

		if(NULL != head) {
			/*UPDATE THE HEAD NODE's TIME*/
			double elapsed = ( end_time.tv_sec - start_time.tv_sec )
  					+ ( end_time.tv_nsec - start_time.tv_nsec )
  					/ 1E9;

  			head->time -= elapsed;
			//printf("%s%.2f%s%.2f\n", "The head's time ", head->time, " Elapsed time: ", elapsed);

			/*HEAD NODE HAS TIMED OUT*/
			while (NULL != head && head->time <= 0) {
				fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s%d%s\n", "seq_num number: ", head->seq_num, " has timed out.");

				//timeout_response.sin_port = htons(head->port);
				send_msg.flag = 2;
				send_msg.seq_num = head->seq_num;

				remove_node(head->seq_num, &head);
				fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s\n", "Timed out node removed.");
				fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s\n", "Status of the delta timer list:");
				print_full_list(head);


				//send message to client tcpd
				sendto(sock, (char*)&send_msg, sizeof send_msg,
					0, (struct sockaddr*)&timeout_response, sizeof timeout_response);

			}
		}

		if(FD_ISSET(sock, &read_set)) {
			if(recvfrom(sock, (char*)&recv_msg, sizeof(recv_msg), 0, NULL, NULL) < 0) {
				perror("There was an error receiving from the driver");
				exit(1);
			}
			if (recv_msg.type == 0) {
				fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s%.2f%s%d\n", "Received add request ", recv_msg.time, ", ", recv_msg.seq_num);
				add_node(recv_msg.time, recv_msg.seq_num, recv_msg.port);
			} else if (recv_msg.type == 1) {
				fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s%d\n", "Received delete request ", recv_msg.seq_num);
				delete_node(recv_msg.seq_num);
			}
			bzero((char*)&recv_msg, sizeof(recv_msg));
			fprintf(stdout, COLOR_DEBUG "[ TIMER ] " COLOR_RESET "%s\n", "Just serviced message");
		}

	}//end for
	close(sock);
	return 0;
}
