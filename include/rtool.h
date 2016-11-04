// RTOOL
// Lots of helpful little things

#ifndef RTOOL_H_
#define RTOOL_H_

/* INCLUDES *******************************************************************/

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>

/* DEFINITIONS ****************************************************************/

#define MSS                         1000
#define LOCALPORT                   9797
#define LOCALADDRESS                "127.0.0.1"
#define ETA                         "164.107.113.23"
#define BETA                        "164.107.113.18"
#define CLIENTTROLLPORT             10001
#define SERVERTROLLPORT             10003
#define TCPDSERVERPORT              10002
#define CBUFFERSIZE                 64000
#define ALPHA                       0.875
#define RHO                         0.25
#define TIMER_PORT                  9090
#define DRIVER_PORT                 8908

#define COLOR_ERROR                 "\e[1m\x1b[31m"     // for printError()
#define COLOR_DEBUG                 "\e[1m\x1b[34m"     // for debug()
#define COLOR_RESET                 "\x1b[0m"

#define ERROR_EXIT                  -1
#define ERROR_UNKNOWN_HOST          -2
#define ERROR_OPENING_SOCKET        -3
#define ERROR_CONNECTING_SOCKET     -4
#define ERROR_WRITING_STRM_SOCKET   -5
#define ERROR_READING_STRM_SOCKET   -6

/* STRUCTURES *****************************************************************/

typedef struct send {
        int flag;
        int seq_num;
        double time;
} send_msg_t;

typedef struct Packet {
        u_short th_sport;
        u_short th_dport;
        int seq;
        int ack;
#if BYTE_ORDER == LITTLE_ENDIAN
        u_int th_x2 : 4,
              th_off : 4;
#endif
#if BYTE_ORDER == BIG_ENDIAN
        u_int th_off : 4,
              th_x2 : 4;
#endif
        u_char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)

        u_short th_win;
        u_short th_sum;
        u_short th_urp;
        
        char body[MSS];
        int bytes_to_read;
        int chksum;
        int packNo;
        int startNo;
        int ackType;
} Packet;

typedef struct MyMessage {
        struct sockaddr_in msg_header;
        struct Packet msg_pack;
        int ackNo;
} MyMessage;

/* BUFFER INFO ****************************************************************/

struct node
{
        int start;
        int nextB;
        int pack;
        int bytes;
        int seq;
        int ack;
        struct timespec time;
        struct node *next;
};

/* DELTA TIMER ****************************************************************/

void starttimer(double timeout,
                int seq_num,
                int sock,
                int ret_port,
                struct sockaddr_in server_addr);
void canceltimer(int seq_num,
                 int sock,
                 struct sockaddr_in server_addr);

/* DELTA TIMER LIST ***********************************************************/

typedef struct message {
        int type;
        int p_num;
        double time;
        int ret_port;
} message_t;

void insertNode(struct node *ptr,
                int start,
                int nextB,
                int pack,
                int bytes,
                int seq,
                int ack,
                struct timespec time);
void deleteNode(struct node *ptr, int start);
void printList(struct node *ptr);
struct node *findNodeBySeq(struct node *ptr, int seq);
struct node *findNode(struct node *ptr, int start);
double calculate_rto(double sample_rtt);
char * GetFromBufferByIndex(int index);
int sendPacket(int seq,
               struct node *temp,
               int troll_sock,
               struct sockaddr_in trolladdr,
               struct sockaddr_in destaddr);

/* CIRCULAR BUFFER ************************************************************/

static char *cBuffer[CBUFFERSIZE];
static int start = 0;
static int end = 0;
static int active = 0;

char * GetFromBuffer();
int AddToBuffer(char *p, struct node *temp);
int getStart();
int getEnd();
void displayBuffer();
int cBufferReady(struct node *temp);
int AddToBufferForServer(char *p);

#endif

#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif
