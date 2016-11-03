#include "../../include/rtool.h"

/* client program called with file name and no args*/
int main(int argc, char *argv[]) {
	/* validate input args */
	if(argc < 1) {
		fprintf(stderr, "Error: Include file name is arguments.\n");
		exit(1);
	}

	int sock, ackSock;                     /* initial socket descriptor */
	int rval;                     /* returned value from a read */
	int num_bytes;		      /* number of bytes of file to be sent */
	int num_read;		      /* bytes read from file stream */
	int num_sent_total = 0;
	int num_sent = 0;
	struct sockaddr_in sin_addr, clientaddr;  /* structure for socket name setup */
	char * file_name = argv[1];   /* file name */
	char buf[MSS] = {0};          /* bytes to send to server */
	struct hostent *hp;	      /* host */
	int ackFlag = 1;

	fprintf(stdout, COLOR_DEBUG "[ CLIENT ] " COLOR_RESET "%s\n\n", "FTP client initialized successfully.");

	/* initialize socket connection in unix domain */
	if((sock = SOCKET(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Error opening socket");
		exit(1);
	}

  	/* construct name of socket to send to */
  	sin_addr.sin_family = AF_INET;
  	sin_addr.sin_port = 0; /* Kernel assign*/
	sin_addr.sin_addr.s_addr = INADDR_ANY; /* Kernel assign */

  	/* establish connection with server */
  	if(CONNECT(sock, (struct sockaddr *)&sin_addr, sizeof(struct sockaddr_in)) < 0)
	{
    		close(sock);
    		perror("Error connecting stream socket");
    		exit(1);
  	}

	/* ACK SOCKET */
	/* This creates a socket to communicate with the local tcpd process */
	if ((ackSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("ackSock socket");
			exit(1);
	}
	bzero((char *)&clientaddr, sizeof clientaddr);
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr(ETA); /* let the kernel fill this in */
	clientaddr.sin_port = htons(10010);
	if (bind(ackSock, (struct sockaddr *)&clientaddr, sizeof clientaddr) < 0) {
		perror("ack bind");
		exit(1);
	}

  	/* Open file for transfer */
  	FILE *fp = fopen(file_name,"rb");
  	if(fp==NULL)
	{
    		perror("Error opening file");
    		exit(1);
  	}
	  char ack[MSS] = {0};
  	/* Read number of bytes in file, rewind file to start index */
	fseek(fp, 0L, SEEK_END);
	num_bytes = ftell(fp);
	rewind(fp);

	/* Send file size in 4 bytes */
	SEND(sock, &num_bytes, MSS, 0);
	fprintf(stdout, COLOR_DEBUG "[ CLIENT ] " COLOR_RESET "FILE SIZE: %i BYTES\n", num_bytes);

	/* Rec ack */
	RECV(ackSock, &ackFlag, sizeof(ackFlag), MSG_WAITALL);
	//printf("Ack Recieved: %d\n\n", ackFlag);
	usleep(1000);
	/* Send file name in 20 bytes */
	SEND(sock, file_name, MSS, 0);
	fprintf(stdout, COLOR_DEBUG "[ CLIENT ] " COLOR_RESET "FILE NAME: %s\n", file_name);

	/* Rec ack */
	RECV(ackSock, &ackFlag, sizeof(ackFlag), MSG_WAITALL);
	//printf("Ack Recieved: %d\n\n", ackFlag);
	usleep(1000);
	bzero(buf, sizeof(buf));

	while((num_read = fread(buf,1,sizeof(buf), fp)) > 0)
	{
		/* send buffer client side tcpd */
		num_sent = SEND(sock, buf, num_read, 0);

		/* Rec ack */
		RECV(ackSock, &ackFlag, sizeof(ackFlag), MSG_WAITALL);
		//printf("Ack Recieved: %d\n\n", ackFlag);

		/* for bookkeeping */
		num_sent_total += num_sent;

		/* added to avoid overloading client side tcpd when sending large files */
		usleep(1000);
	}

	fprintf(stdout, COLOR_DEBUG "[ CLIENT ] " COLOR_RESET "%s\n", "FILE SENT SUCCESSFULLY");

	/* Close file and connection */
	close(fp);
	close(sock);
	return(0);
}
