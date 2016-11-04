// rocket - tcp/udp helper fxns

/* INCLUDES *******************************************************************/

#include "rtool.h"

/* FUNCTION PROTOTYPES ********************************************************/

ssize_t SEND(int socket,
    const void* buffer,
    size_t length,
    int flags);
ssize_t RECV(int socket,
    void* buffer,
    size_t length,
    int flags);
int BIND(int socket,
    struct sockaddr *my_addr,
    socklen_t addrlen);
int ACCEPT(int sock,
    struct sockaddr *address,
    socklen_t *address_len);
int CONNECT(int sockfd,
    const struct sockaddr *addr,
    socklen_t addrlen);
int SOCKET(int domain,
    int type,
    int protocol);

/* IMPLEMENTATIONS ************************************************************/

int ACCEPT(int sock, struct sockaddr *address, socklen_t *address_len) {
	return 0;
}

int CONNECT(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	return 0;
}

int SOCKET(int domain, int type, int protocol) {
	return socket(domain, SOCK_DGRAM, protocol);
}

int BIND(int socket, struct sockaddr *my_addr, socklen_t addrlen) {
        return bind(socket, (struct sockaddr *)my_addr, addrlen);
}

ssize_t SEND(int socket, const void* buffer, size_t length, int flags) {
	struct sockaddr_in daemon_addr;
	daemon_addr.sin_family = AF_INET;
	daemon_addr.sin_port = htons(LOCALPORT);
	daemon_addr.sin_addr.s_addr = INADDR_ANY;
	return sendto(socket, buffer, length, flags,
		(struct sockaddr *)&daemon_addr, sizeof(daemon_addr));
}

ssize_t RECV(int socket, void* buffer, size_t length, int flags) {
	return recvfrom(socket, buffer, length, flags, NULL, NULL);
}
