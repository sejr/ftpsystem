/******************************************************************************/
/*  tcpd.c                                                                    */
/*  ------                                                                    */
/*  TCPD is middleware for our FTP client and server. Basically, since we are */
/*  reverting to UDP in the cli/srv, we implement this daemon to make the     */
/*  transfer reliable.                                                        */
/******************************************************************************/

/* HEADERS AND DEFINITIONS ****************************************************/

#include "../../include/rtool.h"
#include "../../include/crc.c"
#include "../../include/crc.h"

#define Printf if (!qflag) (void)printf
#define Fprintf (void)fprintf

/* HOUSEKEEPING ***************************************************************/

void printUsage()
{
  printf("\nUSAGE:\n"); /* Pulled straight from the README */
  printf("    tcpd --c        to run on client side\n");
  printf("    tcpd --s        to run on server side.\n\n");
}

void printError(char errMsg[], int errorCode)
{
    /* COLORS defined in header; makes error message pop more */
    fprintf(stderr, COLOR_ERROR "ERROR " COLOR_RESET "%s\n", errMsg);
    exit(errorCode); /* Exit with failure */
}

/* Function Declarations ******************************************************/

void    bzero(), bcopy(), exit(), perror();
double  atof();

/* BEGIN tcpd.c ***************************************************************/

int main(int argc, char *argv[])
{

    // Validating arguments
    if (argc != 2)
    {
        printUsage();
        exit(-1);
    }

    // If user has called TCPD on client side:
	if (strncmp(argv[1], "--c", 3) == 0) {

        fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "%s\n\n",
                "Client-side tcpd initialized successfully.");

		// Lots of addresses
		struct sockaddr_in
               trolladdr,
               destaddr,
               localaddr,
               clientaddr,
               tcpdaddr,
               clientack,
               masterAck,
               ackAddr,
               sin_addr,
               timer_addr,
               driver_addr;

		/* Sockets */
		int troll_sock,
            local_sock,
            tcpd_sock,
            ackSock,
            timer_sock;

		/* Variables */
		MyMessage message, ackMessage; 			        /* Packets sent to troll process */
		Packet packet, ackPacket;				        /* Packets sent to server tcpd */
		struct hostent *host; 					        /* Hostname identifier */
		fd_set selectmask; 						        /* Socket descriptor for select */
		int amtFromClient, amtToTroll, total = 0;       /* Bookkeeping vars for sending */
		int chksum = 0;							        /* Checksum */
		char buffer[MSS] = {0};                         /* Local temp buffer */
		int ftpcAck = 1;                                /* tcpd to ftpc ack flag */
		int firstSend = 1;                              /* first send to server tcpd flag */
		int firstRun = 1;								/* first iteration of send loop flag */
		int bufferReady = 0;							/* Buffer management flag */
		double est_rtt = 2.0;                           /* RTT and TRO calcs */
		double est_var = 0.0;							/* RTT and TRO calcs */
		double rto = 3.0;								/* RTT and TRO calcs */
		int current = 0;                                /* Circular buffer indexing vars */
		int next = 0;									/* Circular buffer indexing vars */
		int ack = 0;                                    /* ack var */
		int len = sizeof ackAddr;						/* For rec ack */

		/* Sliding window variables */
		int sn = 0;										/* Packet sequence number */
		int window = 20;								/* Window size */
		int sb = 0;										/* Window base */
		int sm = window-1;								/* Window max */

		/* For rtt and rto calculation */
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
		double elapsed = 0.0;

		/* Init linked list aux structure */
		struct node *start,*temp;
		start = (struct node *)malloc(sizeof(struct node));
		temp = start;
		temp -> next = NULL;

		/* TIMER INIT */
		/* sets up communication to timer process */

		if((timer_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
			perror("Error opening socket");
			exit(1);
		}

		timer_addr.sin_family = AF_INET;
		timer_addr.sin_port = htons(TIMER_PORT); // short, network byte order
		timer_addr.sin_addr.s_addr = inet_addr(ETA);
		memset(&(timer_addr.sin_zero), '\0', 8); // zero the rest of the struct

		driver_addr.sin_family = AF_INET;
		driver_addr.sin_port = htons(DRIVER_PORT); // short, network byte order
		driver_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		memset(&(timer_addr.sin_zero), '\0', 8); // zero the rest of the struct
		if(bind(timer_sock, (struct sockaddr *)&driver_addr, sizeof(struct sockaddr_in)) < 0) {
			perror("Error binding stream socket");
			exit(1);
		}

		/* TROLL ADDRESSS */
		/* this is the addr that troll is running on */

		// uncomment for command line args
		//if ((host = gethostbyname(argv[2])) == NULL) {
		//	printf("Unknown troll host '%s'\n",argv[2]);
		//	exit(1);
		//}
		bzero ((char *)&trolladdr, sizeof trolladdr);
		trolladdr.sin_family = AF_INET;
		//bcopy(host->h_addr, (char*)&trolladdr.sin_addr, host->h_length);
		trolladdr.sin_port = htons(CLIENTTROLLPORT);
		trolladdr.sin_addr.s_addr = inet_addr(ETA);

		/* DESTINATION ADDRESS */
		/* This is the destination address that the troll will forward packets to */

		// uncomment for command line args
		//if ((host = gethostbyname(argv[4])) == NULL) {
		//	printf("Unknown troll host '%s'\n",argv[4]);
		//	exit(1);
		//}
		//bzero ((char *)&destaddr, sizeof destaddr);
		destaddr.sin_family = htons(AF_INET);
		//bcopy(host->h_addr, (char*)&destaddr.sin_addr, host->h_length);
		destaddr.sin_port = htons(TCPDSERVERPORT);
		destaddr.sin_addr.s_addr = inet_addr(BETA);

		/* Client ack address */
		bzero ((char *)&clientack, sizeof clientack);
		clientack.sin_family = AF_INET;
		clientack.sin_port = htons(10010);
		clientack.sin_addr.s_addr = inet_addr(ETA);

		/* SOCKET TO TROLL */
		/* This creates a socket to communicate with the local troll process */

		if ((troll_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("totroll socket");
			exit(1);
		}
		bzero((char *)&localaddr, sizeof localaddr);
		localaddr.sin_family = AF_INET;
		localaddr.sin_addr.s_addr = INADDR_ANY; /* let the kernel fill this in */
		localaddr.sin_port = 0;					/* let the kernel choose a port */
		if (bind(troll_sock, (struct sockaddr *)&localaddr, sizeof localaddr) < 0) {
			perror("client bind");
			exit(1);
		}

		/* SOCKET TO CLIENT */
		/* This creates a socket to communicate with the local troll process */

		if ((local_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("client socket");
			exit(1);
		}
		bzero((char *)&clientaddr, sizeof clientaddr);
		clientaddr.sin_family = AF_INET;
		clientaddr.sin_addr.s_addr = inet_addr(LOCALADDRESS); /* let the kernel fill this in */
		clientaddr.sin_port = htons(LOCALPORT);
		if (bind(local_sock, (struct sockaddr *)&clientaddr, sizeof clientaddr) < 0) {
			perror("client bind");
			exit(1);
		}

		/* MASTER ACK SOCKET */
		/* This creates a socket to communicate with the local ftps process */
		if ((ackSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("ackSock socket");
			exit(1);
		}
		bzero((char *)&masterAck, sizeof masterAck);
		masterAck.sin_family = AF_INET;
		masterAck.sin_addr.s_addr = inet_addr(ETA); /* let the kernel fill this in */
		masterAck.sin_port = htons(10050);
		if (bind(ackSock, (struct sockaddr *)&masterAck, sizeof masterAck) < 0) {
			perror("ack bind");
			exit(1);
		}

		/* SEND DATA TO TROLL */

		/* Initialize checksum table */
		crcInit();

		/* Prepare for select */
		FD_ZERO(&selectmask);
		FD_SET(local_sock, &selectmask);

		/* Begin send loop */
		for(;;) {

			/* Wait for data on socket from cleint */
			if (FD_ISSET(local_sock, &selectmask)) {

				/* GET DATA FRROM CLIENT PROCESS */

				/* If less than 64 slots have been filled allow client data to be copied to buffer */
				/* If > 64 times, this will be skipped until buffer can accept more data */
				if (bufferReady < 64) {

					/* Receive data from ftpc and copy to local buffer */
					amtFromClient = recvfrom(local_sock, buffer, sizeof(buffer), MSG_WAITALL, NULL, NULL);

					if (amtFromClient > 0) {

						fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "Received new data from client.\n");

						/* Copy from local buffer to circular buffer */
						current = AddToBuffer(buffer, temp);// Add to c buffer
						fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "Copied data from ftpc to buffer slot: %d\n", current);

						/* Update aux list info */
						struct timespec temp_t;
						insertNode(temp, current, next, 0, amtFromClient, sn, 0, temp_t);

						/* Send ack to ftpc after data written to buffer */
						sendto(local_sock, (char*)&ftpcAck, sizeof ftpcAck, 0, (struct sockaddr *)&clientack, sizeof clientack);

						/* For bookkeeping/debugging */
						total += amtToTroll;

						/* increment packet seq number */
						sn = sn + 1;

						/* increment count of times data copied to buffer */
						bufferReady = bufferReady+ 1;
					}
				}

				/* If buffer has been written to 64 times continuously check if there is space to copy. If so reset ready flag */
				if (bufferReady == 64) {
					if (cBufferReady(temp) == 1) {
						bufferReady = 0;
					}
				}

				/* Sets timeout for all calls after first iteration. This allows tcpd to wait for first packet from ftpc */
				if (firstRun == 0) {
					if (setsockopt(local_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
						perror("Error");
					}
				}

				/* GET ACK/REQUEST */

				/* Skips to send first packet if in first iteration */
				if (firstRun == 1) {
					firstRun = 0;
				} else {

					/* set timeout on ack socket. This is to prevent lock up if no acks are available */
					if (setsockopt(ackSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
							perror("Error");
					}

					/* Gets ack if available */
					if (recvfrom(ackSock, (char *)&ackMessage, sizeof ackMessage, 0,
					(struct sockaddr *)&ackAddr, &len) > 0) {

						/* Get ack/request no. */
						ack = ackMessage.ackNo;


						/* Successful ack. */
						if (ack == (sb+1)) {

							fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "Successful acknowledgement recieved: %d\n", ack);

							/* Find packet with matching ack and set ack flag to 1 */
							struct node *ptr;
							ptr = (struct node *)malloc(sizeof(struct node));
							ptr->next = NULL;
							ptr = findNodeBySeq(temp, (ack-1));

							/* Stop timer for acked packet. */
							canceltimer((ack-1), timer_sock, timer_addr);

							/* Mark packet as acked in auxlist */
							if (ptr != NULL) {
								ptr->ack = 1;
							}

							/* Adjust window */
							sm = sm + (ack-sb);
							sb = ack;

							/* Calculate rtt/rto/var */

							/* Get time */
							struct timespec endTime;
							clock_gettime(CLOCK_MONOTONIC, &endTime);

							/* find the node that represents the packet that was just acked */
							struct node * acked_node = findNodeBySeq(temp, (ack-1));

							if (NULL != acked_node) {

								/* find elapsed time */
								double elapsed = (endTime.tv_sec - acked_node->time.tv_sec)
								+ (endTime.tv_nsec - acked_node->time.tv_nsec)
								/ 1E9;

								/* rtt/rto for first send */
								if (firstSend == 1) {
									est_rtt = elapsed;
									est_var = elapsed / 2.0;
									rto = est_rtt + 4.0 * est_var;
									firstSend = 0;
								}
								/* rtt/rto for remaining sends */
								else {
									est_rtt = (0.875 * elapsed) + (1 - 0.875) * est_rtt;
									est_var = (1 - 0.25) * est_var + 0.25 * abs((est_rtt - elapsed));
									rto = est_rtt + 4.0 * est_var;
								}

								/* print info */
								//fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "Time Elapsed: %f\n", elapsed);
								//fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "RTO: %.9f\n", rto);
								fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "RTT: %.9f\n", est_rtt);
								//fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "VAR: %.9f\n\n", est_var);

								}
								/* Catch null node */
								else {
									//fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "Acked node does not exist.\n");
								}

						/* NAK recieved. Keep send index at current spot. Redundant assignment for clarity */
						} else {
							fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "Unuccessful acknowledgement recieved: %d\n", ack);
							sb = sb;
						}
					}

					/* Check for timed out nodes */

					/* Set timeout for timeout socket to prevent lockup if timeout is not received */
					if (setsockopt(timer_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
							perror("Error");
					}

					/* timeout message init */
					send_msg_t buffer;
					bzero((char*)&buffer, sizeof(buffer));

					/* Listen for timeout message from timer */
					if (recvfrom(timer_sock, (char*)&buffer, sizeof buffer, MSG_WAITALL, NULL, NULL) < 0) {

						/* if timeout seq no valid resend packet */
						if (buffer.seq_num < sn && buffer.seq_num > sb) {

							/* Create and send packet */
							fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "Packet %d timed out.\n", buffer.seq_num);
							fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "Resending packet: %d\n", buffer.seq_num);
							amtToTroll = sendPacket(buffer.seq_num, temp, troll_sock, trolladdr, destaddr);

							/* Start timer with rto timeout for packet */
							starttimer(rto, buffer.seq_num, timer_sock, DRIVER_PORT, timer_addr);

							if (amtToTroll != sizeof message) {
								perror("totroll sendto");
								exit(1);
							}
						}
					}
				}

				/* if window base valid send packet */
				/* if > sn data for sn has not been read into buffer yet */

				if (sb < sn) {

					/* Create and send packet */
					fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "Sending packet: %d\n", sb);
					amtToTroll = sendPacket(sb, temp, troll_sock, trolladdr, destaddr);

					/* Start timer with rto timeout*/
					starttimer(rto, 1, timer_sock, DRIVER_PORT, timer_addr);

					if (amtToTroll != sizeof message) {
						perror("totroll sendto");
						exit(1);
					}
				}
			}
		}

		/* Reset socket descriptor for select */
		FD_ZERO(&selectmask);
		FD_SET(local_sock, &selectmask);

	/* Run on server side */

    } else if (strncmp(argv[1], "--s", 3) == 0) {

        fprintf(stdout, COLOR_DEBUG "[ TCPD ] " COLOR_RESET "%s\n\n",
                "Server-side tcpd initialized successfully.");

		/* Addresses */
		struct sockaddr_in trolladdr, localaddr, serveraddr, serverack, masterAck;
		int local_sock, ackSock, troll_sock; 						/* sockets */
		MyMessage message, ackMessage; 								/* recieved packet from remote troll process */
		Packet packet, ackPacket;									/* packet struct */
		struct hostent *host; 										/* Hostname identifier */
		fd_set selectmask;											/* Socket descriptor for select */
		int amtFromTcpd, amtToServer, len, total, amtToTcpd = 0; 	/* Bookkeeping vars */
		int chksum, recv_chksum = 0;								/* Checksum */
		int ftpsAck = 1;
													/* ftps ack */
		/* aux List */
		struct node *start,*temp;
		start = (struct node *)malloc(sizeof(struct node));
		temp = start;
		temp -> next = NULL;
		int current = 0;
		int next = 0;
		int ack = 0;
		int rn = 0;

		/* SOCKET FROM TROLL */
		/* This is the socket to recieve from the troll running on the client machine */
		if ((troll_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			printError("Error in troll socket.", ERROR_EXIT);
			exit(1);
		}
		bzero((char *)&localaddr, sizeof localaddr);
		localaddr.sin_family = AF_INET;
		localaddr.sin_addr.s_addr = INADDR_ANY; /* let the kernel fill this in */
		localaddr.sin_port = htons(TCPDSERVERPORT);
		if (bind(troll_sock, (struct sockaddr *)&localaddr, sizeof localaddr) < 0) {
			printError("Binding error.", ERROR_EXIT);
			exit(1);
		}

		/* SOCKET TO SERVER */
		/* This creates a socket to communicate with the local troll process */
		if ((local_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			printError("Error creating socket.", ERROR_EXIT);
			exit(1);
		}

		/* FTPS ACK SOCKET */
		/* This creates a socket to communicate with the local ftps process */
		if ((ackSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			printError("Error creating socket.", ERROR_EXIT);
			exit(1);
		}
		bzero((char *)&serverack, sizeof serverack);
		serverack.sin_family = AF_INET;
		serverack.sin_addr.s_addr = inet_addr(BETA); /* let the kernel fill this in */
		serverack.sin_port = htons(10021);
		if (bind(ackSock, (struct sockaddr *)&serverack, sizeof serverack) < 0) {
			printError("Error binding ack.", ERROR_EXIT);
			exit(1);
		}

		/* ADDRESS TO CONNECT WITH THE SERVER */
		struct sockaddr_in destaddr;
		destaddr.sin_family = AF_INET;
		destaddr.sin_port = htons(LOCALPORT);
		destaddr.sin_addr.s_addr = inet_addr(LOCALADDRESS);

		/* ADDRESS OF CLIENT TCPD */
		struct sockaddr_in clientaddr;
		clientaddr.sin_family = htons(AF_INET);
		clientaddr.sin_port = htons(9999);
		clientaddr.sin_addr.s_addr = inet_addr(ETA);

		/* TROLL ADDRESSS */
		struct sockaddr_in servertrolladdr;
		bzero ((char *)&servertrolladdr, sizeof servertrolladdr);
		servertrolladdr.sin_family = AF_INET;
		servertrolladdr.sin_port = htons(SERVERTROLLPORT);
		servertrolladdr.sin_addr.s_addr = inet_addr(BETA);

		/* MASTER ACK ADDRESS */
		bzero((char *)&masterAck, sizeof masterAck);
		masterAck.sin_family = htons(AF_INET);
		masterAck.sin_addr.s_addr = inet_addr(ETA); /* let the kernel fill this in */
		masterAck.sin_port = htons(10050);


		/* Initialize checksum table */
		crcInit();

		/* Prepare descriptor*/
		FD_ZERO(&selectmask);
		FD_SET(troll_sock, &selectmask);

		/* Begin recieve loop */
		for(;;) {

			/* If data is ready to be recieved from troll on client machien */
			if (FD_ISSET(troll_sock, &selectmask)) {

				/* length of addr for recieve call */
				len = sizeof trolladdr;

					/* read in one packet from the troll */
					amtFromTcpd = recvfrom(troll_sock, (char *)&message, sizeof message, MSG_WAITALL,
					(struct sockaddr *)&trolladdr, &len);
					if (amtFromTcpd < 0) {
						printError("Error receiving packet.", ERROR_EXIT);
						exit(1);
					}

					printf("Recieved data from troll.\n");

					/* Copy packet locally */
					bcopy(&message.msg_pack, &packet, sizeof(packet));

					/* get checksum from packet */
					recv_chksum = packet.chksum;

					/* zero checksum to make equal again */
					packet.chksum = 0;

					/* Calculate checksum of packet recieved */
					chksum = crcFast((char *)&packet, sizeof(packet));
					printf("Checksum of data: %X\n", chksum);

					/*Get packet sequence*/
					int seq = packet.seq;

				/* Compare expected checksum to one caluclated above. Send NAK. */
				if ((chksum != recv_chksum)) {
					printf("Checksum error: Expected: %X Actual: %X\n", recv_chksum, chksum);

					/* Prepare troll wrapper */
					ackMessage.msg_header = masterAck;
					ackMessage.ackNo = rn;

					/* Send NAK */
					sendto(troll_sock, (char *)&ackMessage, sizeof(ackMessage), 0, (struct sockaddr *)&servertrolladdr, sizeof servertrolladdr);
					printf("Sent ack: %d\n\n", rn);

				/* Packet is good. Send ACK */
				} else if ((seq == rn)) {

					/* Add body to circular buffer */
					AddToBufferForServer(packet.body);
					next = getEnd();
					printf("Copied data to buffer slot: %d\n", current);
					struct timespec temp_t_2;
					insertNode(temp, current, next, 0, packet.bytes_to_read, 0, 0, temp_t_2);

					/* Node to get info on current buffer slot */
					struct node *ptr;
					ptr = (struct node *)malloc(sizeof(struct node));
					ptr -> next = NULL;
					ptr = findNode(temp, current);
					int bytesToSend = ptr->bytes;

					/* Forward packet body to server */
					amtToServer = sendto(local_sock, (char *)GetFromBuffer(), bytesToSend, 0, (struct sockaddr *)&destaddr, sizeof destaddr);
					if (amtToServer < 0) {
						printError("Error forwarding packet.", ERROR_EXIT);
					}

					printf("Copied data from buffer slot: %d\n", current);

					printf("Sent data to server.\n");

					/* Get ack from ftps */
					recvfrom(ackSock, &ftpsAck, sizeof(ftpsAck), MSG_WAITALL, NULL, NULL);
					printf("Ack from ftps\n");

					/* Increase expeceted seq no */
					rn = rn + 1;

					/* SEND ACK TO CLIENT TCPD */

					/* Prepare troll wrapper */
					ackMessage.msg_header = masterAck;
					ackMessage.ackNo = rn;

					/* Send Ack */
					sendto(troll_sock, (char *)&ackMessage, sizeof(ackMessage), 0, (struct sockaddr *)&servertrolladdr, sizeof servertrolladdr);
					printf("Sent ack: %d\n\n", rn);

				}
				/* Catch all. Request last packet again */
				else {
					int ack = message.msg_pack.startNo;
					/* Prepare troll wrapper */
					ackMessage.msg_header = masterAck;
					ackMessage.ackNo = rn;
					sendto(troll_sock, (char *)&ackMessage, sizeof(ackMessage), 0, (struct sockaddr *)&servertrolladdr, sizeof servertrolladdr);
					printf("Sent ack: %d\n\n", rn);
				}

				/* Bookkeeping/Debugging */
				total += amtFromTcpd;
			}

			/* Reset decriptor */
			FD_ZERO(&selectmask);
			FD_SET(troll_sock, &selectmask);
		}
	}
}

/* This function sends a packet to the troll process to be forwarded to server side tcpd */
int sendPacket(int seq, struct node *temp, int troll_sock, struct sockaddr_in trolladdr, struct sockaddr_in destaddr) {
	MyMessage message;		        /* Packets sent to troll process */
	Packet packet;			        /* Packets sent to server tcpd */

	/* Node to get info on current buffer slot of seq */
	struct node *ptr;
	ptr = (struct node *)malloc(sizeof(struct node));
	ptr->next = NULL;
	ptr = findNodeBySeq(temp, seq);

	/* get actualy bytes in packet */
	int bytesToSend = ptr->bytes;

	/* Copy payload from circular buffer to tcpd packet */
	bcopy(GetFromBufferByIndex(ptr->start), packet.body, bytesToSend); // removing from c buffer

	/* Prepare packet */
	packet.bytes_to_read = bytesToSend;
	packet.chksum = 0;
	packet.seq = seq;

	/* Calculate checksum */
	int chksum = crcFast((char *)&packet, sizeof(packet));
	printf("Checksum of data: %X\n\n", chksum);

	/* Attach checksum to troll packet */
	/* This is checksum with chksum zerod out. Must do same on rec end */
	packet.chksum = chksum;

	/* Prepare troll wrapper */
	message.msg_pack = packet;
	message.msg_header = destaddr;

	/* record start time */
	struct timespec startTime;
	clock_gettime(CLOCK_MONOTONIC, &startTime);
	ptr->time = startTime;

	/* send packet to troll */
	return sendto(troll_sock, (char *)&message, sizeof message, 0, (struct sockaddr *)&trolladdr, sizeof trolladdr);
}

/*This function creates a start timer message and sends it to the timer process */
void starttimer(double timeout, int seq_num, int sock, int ret_port, struct sockaddr_in server_addr) {
	message_t send_msg;
	bzero((char*)&send_msg, sizeof(send_msg));
	send_msg.type = 0;
	send_msg.p_num = seq_num;
	send_msg.time = timeout;
	send_msg.ret_port = ret_port;

	if(sendto(sock, &send_msg, sizeof(send_msg), 0,
		(struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {

		printError("Error sending to socket in driver.", ERROR_EXIT);
		exit(1);
	}
}


/* This function creates a cancel timer message and sends it to the timer process */
void canceltimer(int seq_num, int sock, struct sockaddr_in server_addr) {
	double elapsed = 0.0;
	message_t send_msg;
	bzero((char*)&send_msg, sizeof(send_msg));
	send_msg.type = 1;
	send_msg.p_num = seq_num;
	send_msg.time = 0;

	if(sendto(sock, &send_msg, sizeof(send_msg), 0,
		(struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {

		perror("There was an error sending to the socket in the driver "
			"(canceltimer)");
		exit(1);
	}
}
