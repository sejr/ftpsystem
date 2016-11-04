/******************************************************************************/
/*  NETWORK PROGRAMMING                                                       */
/*  ------------------------------------------------------------------------- */
/*  Lab 2: File Transfer Application (Server)                                 */
/*  Written by Sam Roth                                                       */
/******************************************************************************/

/* HEADERS AND DEFINITIONS ****************************************************/

#include "../../include/rtool.h"

/* SERVER CODE ****************************************************************/

int main(int argc, char* argv[]) {

    	int sock;
    	struct sockaddr_in sin_addr, serverAck;

        // File info

    	char fileName[20] = {'\0'};
    	int fileSize = 0;
    	char readBuffer[MSS] = {0};
		struct stat st = {0};
		int ackFlag = 1;
		char pathName[] = "recvd/";

        // Let the user know the server has started.

    	fprintf(stdout,
                COLOR_DEBUG "[ SERVER ] "
                COLOR_RESET "FTP server initialized successfully.\n");

    	// Initialize socket

    	if((sock = SOCKET(AF_INET, SOCK_STREAM, 0)) < 0){
    		perror("Error opening socket");
    		exit(1);
    	}

        // Set up local socket
    	sin_addr.sin_family = AF_INET;
    	sin_addr.sin_port = htons(LOCALPORT);
    	sin_addr.sin_addr.s_addr = INADDR_ANY;

    	// Bind name
    	if(BIND(sock,(struct sockaddr *)&sin_addr,sizeof(struct sockaddr_in))<0)
        {
      		perror("Error binding stream socket");
      		exit(1);
    	}

		// Accept connection
		if((ACCEPT(sock, (struct sockaddr *)NULL, (socklen_t *)NULL)) == -1)
        {
				perror("Error connecting stream socket");
				exit(1);
		}

		// Address for ack
		bzero ((char *)&serverAck, sizeof serverAck);
		serverAck.sin_family = AF_INET;
		serverAck.sin_port = htons(10021);
		serverAck.sin_addr.s_addr = inet_addr(BETA);

		// Get file size
		if (RECV(sock, &fileSize, 4, 0, NULL, NULL) < 4) {
			fprintf(stdout, COLOR_DEBUG "[ SERVER ] " COLOR_RESET "%s\n",
                    "Error: The size read returned less than 4");
			exit(1);
		}
		fprintf(stdout, COLOR_DEBUG "[ SERVER ] " COLOR_RESET
                "INCOMING FILE SIZE: %d BYTES\n", fileSize);
		sendto(sock, (char *)&ackFlag, sizeof ackFlag, 0,
               (struct sockaddr *)&serverAck, sizeof serverAck);

		// Get file name
		if (RECV(sock, fileName, sizeof(fileName), 0) < 20) {
			fprintf(stdout, COLOR_DEBUG "[ SERVER ] " COLOR_RESET "%s\n",
                    "Error: The name read returned less than 20");
			exit(1);
		}
		fprintf(stdout, COLOR_DEBUG "[ SERVER ] " COLOR_RESET
                "INCOMING FILE NAME: %s\n", fileName);
		sendto(sock, (char *)&ackFlag, sizeof ackFlag, 0,
               (struct sockaddr *)&serverAck, sizeof serverAck);

		// Create output directory
		if (stat("recvd", &st) == -1) {
				mkdir("recvd", 0700);
		}

		FILE* output = fopen(strcat(pathName, fileName), "wb");

		if (NULL == output) {
			fprintf(stderr, "%s\n", "Error opening the output file");
			exit(1);
		}

		// Read file from server-side tcpd
		int amtReadTotal = 0;
		int amtRead = 0;
		while (amtReadTotal < fileSize) {
				amtRead = RECV(sock,readBuffer,sizeof(readBuffer),MSG_WAITALL);
				amtReadTotal += amtRead;
				if (amtRead < 0) {
					fprintf(stderr,
                            "%s\n\n",
                            "Error reading from the connection stream.
                            Server terminating");
					exit(1);
				}
				sendto(sock,
                       (char *)&ackFlag,
                       sizeof ackFlag,
                       0,
                       (struct sockaddr *)&serverAck,
                       sizeof serverAck);

				fwrite(readBuffer, 1, amtRead, output);
		}

        // File transfer complete
		fprintf(stdout, COLOR_DEBUG "[ SERVER ] " COLOR_RESET
                "RECEIVED FILE SUCCESSFULLY.\n");

		// Close everything
		close(sock);
		fclose(output);

		return 0;
}

/* END ftps.c *****************************************************************/
