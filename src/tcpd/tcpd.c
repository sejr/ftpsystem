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

#define Printf if (!qflag)(void) printf
#define Fprintf(void) fprintf

/* HOUSEKEEPING ***************************************************************/

void printUsage() {
    printf("\nUSAGE:\n"); /* Pulled straight from the README */
    printf("    tcpd --c        to run on client side\n");
    printf("    tcpd --s        to run on server side.\n\n");
}

void printError(char errMsg[], int errorCode) {
    /* COLORS defined in header; makes error message pop more */
    fprintf(stderr, COLOR_ERROR "ERROR "
        COLOR_RESET "%s\n", errMsg);
    exit(errorCode); /* Exit with failure */
}

/* Function Declarations ******************************************************/

void bzero(), bcopy(), exit(), perror();
double atof();

/* BEGIN tcpd.c ***************************************************************/

int main(int argc, char * argv[]) {
    // Validating arguments
    if (argc != 2) {
        printUsage();
        exit(-1);
    }

    // If user has called TCPD on client side:
    if (strncmp(argv[1], "--c", 3) == 0) {

        fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
            COLOR_RESET "%s\n\n",
            "Client-side tcpd initialized successfully.");

        // Socket descriptors
        int trollSocket,
        localSocket,
        tcpdSocket,
        ackSocket,
        timerSocket;

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
        sinAddr,
        timerAddr,
        driverAddr;

        // RTT / RTO calcuations
        double est_rtt = 2.0;
        double est_var = 0.0;
        double rto = 3.0;
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 300000;
        double elapsed = 0.0;

        struct timeval tv_ftpc;
        tv_ftpc.tv_sec = 0;
        tv_ftpc.tv_usec = 50000;

        // Misc
        MyMessage message, ackMessage;
        Packet packet, ackPacket;
        struct hostent * host;
        fd_set selectmask;
        int amtFromClient, amtToTroll, total = 0;
        int chksum = 0;
        char buffer[MSS] = {
            0
        };
        int ftpcAck = 1;
        int firstSend = 1;
        int firstRun = 1;
        int bufferReady = 0;
        int current = 0;
        int next = 0;
        int ack = 0;
        int len = sizeof ackAddr;

        // Sliding window
        int sn = 0;
        int window = 20;
        int sb = 0;
        int sm = window - 1;

        // List struct
        struct node * start, * temp;
        start = (struct node * ) malloc(sizeof(struct node));
        temp = start;
        temp - > next = NULL;

        // Initialize communication to timer process.
        if ((timerSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("Error opening socket");
            exit(1);
        }

        // Set up timer socket.
        timerAddr.sin_family = AF_INET;
        timerAddr.sin_port = htons(TIMER_PORT);
        timerAddr.sinAddr.s_addr = inet_addr(ETA);
        memset( & (timerAddr.sin_zero), '\0', 8);

        // Set up driver socket.
        driverAddr.sin_family = AF_INET;
        driverAddr.sin_port = htons(DRIVER_PORT);
        driverAddr.sinAddr.s_addr = htonl(INADDR_ANY);
        memset( & (timerAddr.sin_zero), '\0', 8);
        if (bind(timerSocket,
                (struct sockaddr * ) & driverAddr,
                sizeof(struct sockaddr_in)) < 0) {
            perror("Error binding stream socket");
            exit(1);
        }

        // Setting up client troll.
        bzero((char * ) & trolladdr, sizeof trolladdr);
        trolladdr.sin_family = AF_INET;
        trolladdr.sin_port = htons(CLIENTTROLLPORT);
        trolladdr.sinAddr.s_addr = inet_addr(ETA);

        // Setting up server tcpd for troll.
        destaddr.sin_family = htons(AF_INET);
        destaddr.sin_port = htons(TCPDSERVERPORT);
        destaddr.sinAddr.s_addr = inet_addr(BETA);

        // Setting up socket for acks.
        bzero((char * ) & clientack, sizeof clientack);
        clientack.sin_family = AF_INET;
        clientack.sin_port = htons(10010);
        clientack.sinAddr.s_addr = inet_addr(ETA);

/* BEGIN socket to client troll ***********************************************/

        if ((trollSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("totroll socket");
            exit(1);
        }
        bzero((char * ) & localaddr, sizeof localaddr);
        localaddr.sin_family = AF_INET;
        localaddr.sinAddr.s_addr = INADDR_ANY;
        localaddr.sin_port = 0;
        if (bind(trollSocket,
                (struct sockaddr * ) & localaddr,
                sizeof localaddr) < 0) {
            perror("client bind");
            exit(1);
        }

        if ((localSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("client socket");
            exit(1);
        }
        bzero((char * ) & clientaddr, sizeof clientaddr);
        clientaddr.sin_family = AF_INET;
        clientaddr.sinAddr.s_addr = inet_addr(LOCALADDRESS);
        clientaddr.sin_port = htons(LOCALPORT);
        if (bind(localSocket,
                (struct sockaddr * ) & clientaddr,
                sizeof clientaddr) < 0) {
            perror("client bind");
            exit(1);
        }

        if ((ackSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("ackSocket socket");
            exit(1);
        }
        bzero((char * ) & masterAck, sizeof masterAck);
        masterAck.sin_family = AF_INET;
        masterAck.sinAddr.s_addr = inet_addr(ETA);
        masterAck.sin_port = htons(10050);
        if (bind(ackSocket,(struct sockaddr*) &masterAck, sizeof masterAck)<0) {
            perror("ack bind");
            exit(1);
        }

/* SEND DATA to client-side troll *********************************************/

        crcInit();
        FD_ZERO( & selectmask);
        FD_SET(localSocket, & selectmask);

        for (;;) {
            if (FD_ISSET(localSocket, & selectmask)) {
                if (bufferReady < 64) {
                    amtFromClient = recvfrom(localSocket,
                        buffer,
                        sizeof(buffer),
                        MSG_WAITALL,
                        NULL,
                        NULL);
                    if (amtFromClient > 0) {
                        fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                            COLOR_RESET "NEW DATA FROM CLIENT!\n");
                        current = AddToBuffer(buffer, temp);
                        fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                            COLOR_RESET "DATA -> BUFFER SLOT: %d\n",
                            current);
                        struct timespec temp_t;
                        insertNode(temp, current, next, 0,
                            amtFromClient, sn, 0, temp_t);
                        sendto(localSocket,
                            (char * ) & ftpcAck,
                            sizeof ftpcAck,
                            0,
                            (struct sockaddr * ) & clientack,
                            sizeof clientack);

                        total += amtToTroll;
                        sn = sn + 1;
                        bufferReady = bufferReady + 1;
                    }
                }

                if (bufferReady == 64) {
                    if (cBufferReady(temp) == 1) {
                        bufferReady = 0;
                    }
                }

                if (firstRun == 0) {
                    if (setsockopt(localSocket,
                        SOL_SOCKET,
                        SO_RCVTIMEO,
                        & tv_ftpc,
                        sizeof(tv_ftpc)) < 0) {
                        perror("Error");
                    }
                }

                if (firstRun == 1) {
                    firstRun = 0;
                } else {
                    if (setsockopt(ackSocket,
                            SOL_SOCKET,
                            SO_RCVTIMEO, & tv,
                            sizeof(tv)) < 0) {
                        perror("Error");
                    }
                    if (recvfrom(ackSocket,
                        (char * ) & ackMessage,
                        sizeof ackMessage,
                        0,
                        (struct sockaddr * ) & ackAddr,
                        & len) > 0) {

                        ack = ackMessage.ackNo;
                        if (ack == (sb + 1)) {
                            fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                                COLOR_RESET "ACK RECEIVED: # %d\n", ack);
                            struct node * ptr;
                            ptr = (struct node * ) malloc(sizeof(struct node));
                            ptr - > next = NULL;
                            ptr = findNodeBySeq(temp, (ack - 1));
                            canceltimer((ack - 1), timerSocket, timerAddr);
                            if (ptr != NULL) {
                                ptr - > ack = 1;
                            }

                            sm = sm + (ack - sb);
                            sb = ack;
                            struct timespec endTime;
                            clock_gettime(CLOCK_MONOTONIC, & endTime);

                            struct node * acked_node = findNodeBySeq(temp,
                                (ack - 1));

                            if (NULL != acked_node) {
                                double elapsed = (endTime.tv_sec -
                                    acked_node - > time.tv_sec) +
                                    (endTime.tv_nsec - acked_node - >
                                    time.tv_nsec) / 1E9;
                                if (firstSend == 1) {
                                    est_rtt = elapsed;
                                    est_var = elapsed / 2.0;
                                    rto = est_rtt + 4.0 * est_var;
                                    firstSend = 0;
                                }
                                else {
                                    est_rtt = (0.875 * elapsed) +
                                    (1 - 0.875) * est_rtt;
                                    est_var = (1 - 0.25) * est_var + 0.25 *
                                    abs((est_rtt - elapsed));
                                    rto = est_rtt + 4.0 * est_var;
                                }
                                fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                                    COLOR_RESET "RTT: %.9f\n", est_rtt);
                            }
                        } else {
                            if (ack < sn) {
                                fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                                    COLOR_RESET "NAK RECEIVED: # %d\n", ack);
                            }
                            sb = sb;
                        }
                    }

                    // CHECK FOR TIMED OUT NODES
                    if (setsockopt(timerSocket,
                        SOL_SOCKET,
                        SO_RCVTIMEO,
                        & tv,
                        sizeof(tv)) < 0) {
                        perror("Error");
                    }

                    send_msg_t buffer;
                    bzero((char * ) & buffer, sizeof(buffer));

                    if (recvfrom(timerSocket,
                        (char * ) & buffer,
                        sizeof buffer,
                        MSG_WAITALL,
                        NULL,
                        NULL) > 0) {

                        // IF SEQ # INVALID RETRANSMIT
                        if (buffer.seq_num < sn && buffer.seq_num >= sb) {
                            fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                                COLOR_RESET "PACKET SEQ # %d TIMED OUT.\n",
                                buffer.seq_num);
                            fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                                COLOR_RESET "RETRANSMITTING SEQ # %d\n",
                                buffer.seq_num);
                            amtToTroll = sendPacket(buffer.seq_num, temp,
                                trollSocket, trolladdr, destaddr);

                            starttimer(rto, buffer.seq_num, timerSocket,
                                DRIVER_PORT, timerAddr);
                            if (amtToTroll != sizeof message) {
                                perror("totroll sendto");
                                exit(1);
                            }
                        }
                    }
                }
                if (sb < sn) {
                    fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                        COLOR_RESET "TRANSMITTING PACKET %d\n", sb);
                    amtToTroll = sendPacket(sb, temp, trollSocket,
                        trolladdr, destaddr);

                    /* Start timer with rto timeout*/
                    starttimer(rto, sb, timerSocket, DRIVER_PORT, timerAddr);

                    if (amtToTroll != sizeof message) {
                        perror("totroll sendto");
                        exit(1);
                    }
                }
            }
        }

        /* Reset socket descriptor for select */
        FD_ZERO( & selectmask);
        FD_SET(localSocket, & selectmask);

        /* Run on server side */

    } else if (strncmp(argv[1], "--s", 3) == 0) {

        fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
            COLOR_RESET "%s\n\n",
            "Server-side tcpd initialized successfully.");

        // Addresses
        struct sockaddr_in trolladdr,
        localaddr,
        serveraddr,
        serverack,
        masterAck;
        // Socket descriptors
        int localSocket,
        ackSocket,
        trollSocket;
        MyMessage message, ackMessage;
        Packet packet, ackPacket;
        struct hostent * host;
        fd_set selectmask;
        int amtFromTcpd, amtToServer, len, total, amtToTcpd = 0;
        int chksum, recv_chksum = 0;
        int ftpsAck = 1;
        /* ftps ack */
        /* aux List */
        struct node * start, * temp;
        start = (struct node * ) malloc(sizeof(struct node));
        temp = start;
        temp - > next = NULL;
        int current = 0;
        int next = 0;
        int ack = 0;
        int rn = 0;

        // SOCKET FROM TROLL
        if ((trollSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            printError("Error in troll socket.", ERROR_EXIT);
            exit(1);
        }
        bzero((char * ) & localaddr, sizeof localaddr);
        localaddr.sin_family = AF_INET;
        localaddr.sinAddr.s_addr = INADDR_ANY;
        localaddr.sin_port = htons(TCPDSERVERPORT);
        if (bind(trollSocket,
            (struct sockaddr * ) & localaddr,
            sizeof localaddr) < 0) {
            printError("Binding error.", ERROR_EXIT);
            exit(1);
        }

        // SOCKET TO SERVER
        if ((localSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            printError("Error creating socket.", ERROR_EXIT);
            exit(1);
        }

        // ACK
        if ((ackSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            printError("Error creating socket.", ERROR_EXIT);
            exit(1);
        }
        bzero((char * ) & serverack, sizeof serverack);
        serverack.sin_family = AF_INET;
        serverack.sinAddr.s_addr = inet_addr(BETA);
        serverack.sin_port = htons(10021);
        if (bind(ackSocket,
            (struct sockaddr * ) & serverack,
            sizeof serverack) < 0) {
            printError("Error binding ack.", ERROR_EXIT);
            exit(1);
        }

        // Connect to server
        struct sockaddr_in destaddr;
        destaddr.sin_family = AF_INET;
        destaddr.sin_port = htons(LOCALPORT);
        destaddr.sinAddr.s_addr = inet_addr(LOCALADDRESS);

        // Client tcpd info
        struct sockaddr_in clientaddr;
        clientaddr.sin_family = htons(AF_INET);
        clientaddr.sin_port = htons(9999);
        clientaddr.sinAddr.s_addr = inet_addr(ETA);

        // Troll address
        struct sockaddr_in servertrolladdr;
        bzero((char * ) & servertrolladdr, sizeof servertrolladdr);
        servertrolladdr.sin_family = AF_INET;
        servertrolladdr.sin_port = htons(SERVERTROLLPORT);
        servertrolladdr.sinAddr.s_addr = inet_addr(BETA);

        // Ack address
        bzero((char * ) & masterAck, sizeof masterAck);
        masterAck.sin_family = htons(AF_INET);
        masterAck.sinAddr.s_addr = inet_addr(ETA);
        masterAck.sin_port = htons(10050);

        crcInit();
        FD_ZERO( & selectmask);
        FD_SET(trollSocket, & selectmask);

        for (;;) {

            // Are we ready?
            if (FD_ISSET(trollSocket, & selectmask)) {
                len = sizeof trolladdr;

                // Read packet in
                amtFromTcpd = recvfrom(trollSocket,
                    (char * ) & message,
                    sizeof message,
                    MSG_WAITALL,
                    (struct sockaddr * ) & trolladdr, & len);
                if (amtFromTcpd < 0) {
                    printError("Error receiving packet.",
                        ERROR_EXIT);
                    exit(1);
                }

                fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                    COLOR_RESET "NEW DATA FROM TROLL!\n");
                bcopy( & message.msg_pack, & packet, sizeof(packet));
                recv_chksum = packet.chksum;
                packet.chksum = 0;

                // Calculate received checksum
                chksum = crcFast((char * ) & packet, sizeof(packet));
                fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                    COLOR_RESET "CRC CHECKSUM: %X\n", chksum);
                int seq = packet.seq;

                // PACKET IS NOT OKAY
                if ((chksum != recv_chksum)) {
                    fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                        COLOR_RESET "CRC ERROR! Expected: %X Actual: %X\n",
                        recv_chksum, chksum);
                    ackMessage.msg_header = masterAck;
                    ackMessage.ackNo = rn;
                    sendto(trollSocket,
                        (char * ) & ackMessage,
                        sizeof(ackMessage),
                        0,
                        (struct sockaddr * ) & servertrolladdr,
                        sizeof servertrolladdr);
                    fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                        COLOR_RESET "ACK TRANSMITTED TO CLIENT: %d\n", rn);

                    // PACKET IS OKAY
                } else if ((seq == rn)) {

                    // Add to circular buffer
                    AddToBufferForServer(packet.body);
                    next = getEnd();
                    struct timespec temp_t_2;
                    insertNode(temp, current, next, 0,
                        packet.bytes_to_read, 0, 0, temp_t_2);

                    /* Node to get info on current buffer slot */
                    struct node * ptr;
                    ptr = (struct node * ) malloc(sizeof(struct node));
                    ptr - > next = NULL;
                    ptr = findNode(temp, current);
                    int bytesToSend = ptr - > bytes;

                    /* Forward packet body to server */
                    amtToServer = sendto(localSocket,
                        (char * ) GetFromBuffer(),
                        bytesToSend,
                        0,
                        (struct sockaddr * ) & destaddr,
                        sizeof destaddr);
                    if (amtToServer < 0) {
                        printError("Error forwarding packet.", ERROR_EXIT);
                    }

                    fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                        COLOR_RESET "DATA FORWARDED TO FTP SERVER\n");
                    recvfrom(ackSocket, & ftpsAck, sizeof(ftpsAck),
                        MSG_WAITALL, NULL, NULL);
                    fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                        COLOR_RESET "FTP SERVER ACK RECEIVED\n");
                    rn = rn + 1;
                    ackMessage.msg_header = masterAck;
                    ackMessage.ackNo = rn;
                    sendto(trollSocket,
                        (char * ) & ackMessage,
                        sizeof(ackMessage),
                        0,
                        (struct sockaddr * ) & servertrolladdr,
                        sizeof servertrolladdr);
                    fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                        COLOR_RESET "TRANSMITTED ACK %d\n", rn);
                } else {
                    int ack = message.msg_pack.startNo;
                    ackMessage.msg_header = masterAck;
                    ackMessage.ackNo = rn;
                    sendto(trollSocket,
                        (char * ) & ackMessage,
                        sizeof(ackMessage),
                        0,
                        (struct sockaddr * ) & servertrolladdr,
                        sizeof servertrolladdr);
                    fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
                        COLOR_RESET "TRANSMITTED ACK %d\n", rn);
                }

                /* Bookkeeping/Debugging */
                total += amtFromTcpd;
            }

            /* Reset decriptor */
            FD_ZERO( & selectmask);
            FD_SET(trollSocket, & selectmask);
        }
    }
}

/* FUNCTION IMPLEMENTATIONS ***************************************************/

// Send packet to troll
int sendPacket(int seq,
    struct node * temp,
    int trollSocket,
    struct sockaddr_in trolladdr,
    struct sockaddr_in destaddr) {
    MyMessage message;
    Packet packet;
    struct node * ptr;
    ptr = (struct node * ) malloc(sizeof(struct node));
    ptr - > next = NULL;
    ptr = findNodeBySeq(temp, seq);
    int bytesToSend = ptr - > bytes;
    bcopy(GetFromBufferByIndex(ptr - > start), packet.body, bytesToSend);
    packet.bytes_to_read = bytesToSend;
    packet.chksum = 0;
    packet.seq = seq;

    // CRC CHECKSUM CALCULATION
    int chksum = crcFast((char * ) & packet, sizeof(packet));
    fprintf(stdout, COLOR_DEBUG "[ TCPD ] "
        COLOR_RESET "CRC CHECKSUM: %X\n", chksum);

    packet.chksum = chksum;
    message.msg_pack = packet;
    message.msg_header = destaddr;
    struct timespec startTime;
    clock_gettime(CLOCK_MONOTONIC, & startTime);
    ptr - > time = startTime;

    // Off it goes!
    return sendto(trollSocket,
        (char * ) & message,
        sizeof message,
        0,
        (struct sockaddr * ) & trolladdr,
        sizeof trolladdr);
}

// Starts timer
void starttimer(double timeout,
    int seq_num,
    int sock,
    int ret_port,
    struct sockaddr_in server_addr) {
    message_t send_msg;
    bzero((char * ) & send_msg, sizeof(send_msg));
    send_msg.type = 0;
    send_msg.p_num = seq_num;
    send_msg.time = timeout;
    send_msg.ret_port = ret_port;
    if (sendto(sock, & send_msg, sizeof(send_msg), 0,
            (struct sockaddr * ) & server_addr, sizeof(server_addr)) < 0) {
        printError("Error sending to socket in driver.", ERROR_EXIT);
        exit(1);
    }
}

// Cancels timer
void canceltimer(int seq_num, int sock, struct sockaddr_in server_addr) {
    double elapsed = 0.0;
    message_t send_msg;
    bzero((char * ) & send_msg, sizeof(send_msg));
    send_msg.type = 1;
    send_msg.p_num = seq_num;
    send_msg.time = 0;
    if (sendto(sock, & send_msg, sizeof(send_msg), 0,
            (struct sockaddr * ) & server_addr, sizeof(server_addr)) < 0) {
        perror("There was an error sending to the socket in the driver "
            "(canceltimer)");
        exit(1);
    }
}

/* END tcpd.c *****************************************************************/
