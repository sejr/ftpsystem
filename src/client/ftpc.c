/******************************************************************************/
/*  NETWORK PROGRAMMING                                                       */
/*  ------------------------------------------------------------------------- */
/*  Lab 2: File Transfer Application (Client)                                 */
/*  Written by Sam Roth                                                       */
/******************************************************************************/

/* HEADERS AND DEFINITIONS ****************************************************/

#include "../../include/rtool.h"

/* CLIENT CODE ****************************************************************/

int main(int argc, char *argv[]) {
	if(argc < 1) {
		fprintf(stderr, "usage: client <file_name>\n"); // should fix to path
		exit(1);
	}

	int sock, ackSock;
	int rval;
	int num_bytes;
	int num_read;
	int num_sent_total = 0;
	int num_sent = 0;
	struct sockaddr_in sin_addr, clientaddr;
	char * file_name = argv[1];
	char buf[MSS] = {0};
	struct hostent *hp;
	int ackFlag = 1;

	fprintf(stdout, COLOR_DEBUG "[ CLIENT ] " COLOR_RESET "%s\n",
            "FTP client initialized successfully.");

	// init socket
	if((sock = SOCKET(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Error opening socket");
		exit(1);
	}

  	// Construct name
  	sin_addr.sin_family = AF_INET;
  	sin_addr.sin_port = 0; /* Kernel assign*/
	sin_addr.sin_addr.s_addr = INADDR_ANY; /* Kernel assign */

  	// Connect to socket
  	if(CONNECT(sock,(struct sockaddr *)&sin_addr,sizeof(struct sockaddr_in))<0)
	{
    		close(sock);
    		perror("Error connecting stream socket");
    		exit(1);
  	}

    // TCPD socket for acks
	if ((ackSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("ackSock socket");
			exit(1);
	}

    // Another address
	bzero((char *)&clientaddr, sizeof clientaddr);
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr(ETA);
	clientaddr.sin_port = htons(10010);
	if (bind(ackSock, (struct sockaddr *)&clientaddr, sizeof clientaddr) < 0) {
		perror("ack bind");
		exit(1);
	}

  	// Open file
  	FILE *fp = fopen(file_name,"rb");
  	if(fp==NULL)
	{
    		perror("Error opening file");
    		exit(1);
  	}
    char ack[MSS] = {0};

    // Get file size
	fseek(fp, 0L, SEEK_END);
	num_bytes = ftell(fp);
	rewind(fp);

	// Send file size
	SEND(sock, &num_bytes, MSS, 0);
	fprintf(stdout, COLOR_DEBUG "[ CLIENT ] " COLOR_RESET
            "FILE SIZE: %i BYTES\n", num_bytes);

    // File size ack
	RECV(ackSock, &ackFlag, sizeof(ackFlag), MSG_WAITALL);

    // Send file name
	SEND(sock, file_name, MSS, 0);
	fprintf(stdout, COLOR_DEBUG "[ CLIENT ] " COLOR_RESET
            "FILE NAME: %s\n", file_name);

	// File name ack
	RECV(ackSock, &ackFlag, sizeof(ackFlag), MSG_WAITALL);
	bzero(buf, sizeof(buf));

    // Send file data
	while((num_read = fread(buf,1,sizeof(buf), fp)) > 0)
	{
		num_sent = SEND(sock, buf, num_read, 0);
		RECV(ackSock, &ackFlag, sizeof(ackFlag), MSG_WAITALL);
		num_sent_total += num_sent;
	}

    // File sent. close client sockets.

	fprintf(stdout, COLOR_DEBUG "[ CLIENT ] " COLOR_RESET "%s\n",
            "FILE SENT SUCCESSFULLY");

	close(fp);
	close(sock);
	return(0);
}

/* END client.c ***************************************************************/
