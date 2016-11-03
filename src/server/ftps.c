#include "../../include/rtool.h"

int main(int argc, char* argv[]) {

    	int sock;                     /* initial socket descriptor */
    	struct sockaddr_in sin_addr, serverAck; /* structure for socket name setup */

    	char fileName[20] = {'\0'};
    	int fileSize = 0;
    	char readBuffer[MSS] = {0};
		struct stat st = {0};
		int ackFlag = 1;
		char pathName[] = "recvd/";
    	printf("TCP server waiting for remote connection from clients ...\n\n");

    	/*initialize socket connection in unix domain*/
    	if((sock = SOCKET(AF_INET, SOCK_STREAM, 0)) < 0){
    		perror("Error opening socket");
    		exit(1);
    	}
    	sin_addr.sin_family = AF_INET;
    	sin_addr.sin_port = htons(LOCALPORT);
    	sin_addr.sin_addr.s_addr = INADDR_ANY;

    	/* bind socket name to socket */
    	if(BIND(sock, (struct sockaddr *)&sin_addr, sizeof(struct sockaddr_in)) < 0) {
      		perror("Error binding stream socket");
      		exit(1);
    	}

		/* accept a connection in socket msgsocket */
		if((ACCEPT(sock, (struct sockaddr *)NULL, (socklen_t *)NULL)) == -1) {
				perror("Error connecting stream socket");
				exit(1);
		}

		/* address to send ack back to tcpd */
		bzero ((char *)&serverAck, sizeof serverAck);
		serverAck.sin_family = AF_INET;
		serverAck.sin_port = htons(10021);
		serverAck.sin_addr.s_addr = inet_addr(BETA);

		/* get the size of the payload */
		if (RECV(sock, &fileSize, 4, 0, NULL, NULL) < 4) {
			printf("%s\n", "Error: The size read returned less than 4");
			exit(1);
		}
		printf("Recieved size: %d bytes\n\n", fileSize);
		/* Send ack to tcpd */
		sendto(sock, (char *)&ackFlag, sizeof ackFlag, 0, (struct sockaddr *)&serverAck, sizeof serverAck);

		/* get the name of the file */
		if (RECV(sock, fileName, sizeof(fileName), 0) < 20) {
			printf("%s\n", "Error: The name read returned less than 20");
			exit(1);
		}
		printf("Received name: %s\n\n", fileName);
		/* Send ack to tcpd */
		sendto(sock, (char *)&ackFlag, sizeof ackFlag, 0, (struct sockaddr *)&serverAck, sizeof serverAck);

		/* create a directory if one does not already exist */
		if (stat("recvd", &st) == -1) {
				mkdir("recvd", 0700);
		}

		FILE* output = fopen(strcat(pathName, fileName), "wb");

		if (NULL == output) {
			fprintf(stderr, "%s\n", "Error opening the output file");
			exit(1);
		}

		/* read the payload from the stream until the whole payload has been read */
		int amtReadTotal = 0;
		int amtRead = 0;
		while (amtReadTotal < fileSize) {
				/* Receive from server side tcpd */
				amtRead = RECV(sock, readBuffer, sizeof(readBuffer), MSG_WAITALL);

				/* track byes recieved */
				amtReadTotal += amtRead;
				if (amtRead < 0) {
					fprintf(stderr, "%s\n\n", "Error reading from the connection stream. Server terminating");
					exit(1);
				}
				/* Send ack to tcpd */
				sendto(sock, (char *)&ackFlag, sizeof ackFlag, 0, (struct sockaddr *)&serverAck, sizeof serverAck);


				/* write the received data to the output file */
				fwrite(readBuffer, 1, amtRead, output);
		}

		printf("Recieved file.\n");

		/* close the output file and connections */
		close(sock);
		fclose(output);

		return 0;
}
