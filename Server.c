#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <net/if.h>

#define BUFSIZE 1024
#define MAXPENDING 5

#define MCAST_PORT 5000
#define MCAST_IPV4 "239.1.1.1"
#define MCAST_IPV6 "ff02::1234"


void printTimestamp(void){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    printf("[%02d:%02d:%02d] ",
           t->tm_hour,
           t->tm_min,
           t->tm_sec);
}

//    Create IPv4 TCP listening socket
int createTCP4Socket(in_port_t port){
    int sockfd;
    struct sockaddr_in servAddr;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sockfd < 0) {
        perror("socket IPv4 failed");
        exit(EXIT_FAILURE);
    }

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               &yes, sizeof(yes));

    memset(&servAddr, 0, sizeof(servAddr));

    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    if (bind(sockfd,
             (struct sockaddr *)&servAddr,
             sizeof(servAddr)) < 0) {
        perror("bind IPv4 failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, MAXPENDING) < 0) {
        perror("listen IPv4 failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    return sockfd;
}


//    Create IPv6 TCP listening socket
int createTCP6Socket(in_port_t port){
    int sockfd;
    struct sockaddr_in6 servAddr;

    sockfd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    if (sockfd < 0) {
        perror("socket IPv6 failed");
        exit(EXIT_FAILURE);
    }

    int yes = 1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               &yes, sizeof(yes));

    // Keeping IPv4 and IPv6 as separate sockets.

    setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY,
           &yes, sizeof(yes));

    memset(&servAddr, 0, sizeof(servAddr));

    servAddr.sin6_family = AF_INET6;
    servAddr.sin6_addr = in6addr_any;
    servAddr.sin6_port = htons(port);

    if (bind(sockfd,
             (struct sockaddr *)&servAddr,
             sizeof(servAddr)) < 0) {
        perror("bind IPv6 failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, MAXPENDING) < 0) {
        perror("listen IPv6 failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

//Accept a new TCP client
int acceptClient(int servSock){
    struct sockaddr_storage clntAddr;
    socklen_t clntAddrLen = sizeof(clntAddr);

    int clntSock = accept(
        servSock,
        (struct sockaddr *)&clntAddr,
        &clntAddrLen
    );

    if (clntSock < 0) {
        perror("accept failed");
        return -1;
    }

    char host[NI_MAXHOST];
    char service[NI_MAXSERV];

    int err = getnameinfo(
        (struct sockaddr *)&clntAddr,
        clntAddrLen,
        host,
        sizeof(host),
        service,
        sizeof(service),
        NI_NUMERICHOST | NI_NUMERICSERV
    );

    printTimestamp();

    if (err == 0) {
        printf("Client connected: %s:%s\n",
               host, service);
    } else {
        printf("Client connected\n");
    }

    return clntSock;
}

//    Create IPv4 multicast receiving socket
int createMulticast4Socket(void){
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("IPv4 multicast socket failed");
        exit(EXIT_FAILURE);
    }

    int yes = 1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               &yes, sizeof(yes));

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(MCAST_PORT);

    if (bind(sockfd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
        perror("bind multicast IPv4 failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    //   Join multicast group 239.1.1.1
    
    struct ip_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));

    inet_pton(AF_INET,
              MCAST_IPV4,
              &mreq.imr_multiaddr);

    //    INADDR_ANY = let OS choose interface
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sockfd,
                   IPPROTO_IP,
                   IP_ADD_MEMBERSHIP,
                   &mreq,
                   sizeof(mreq)) < 0) {
        perror("IPv4 multicast join failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

//    Create IPv6 multicast receiving socket
int createMulticast6Socket(void){
    int sockfd;

    sockfd = socket(AF_INET6, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("IPv6 multicast socket failed");
        exit(EXIT_FAILURE);
    }

    int yes = 1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               &yes, sizeof(yes));

    struct sockaddr_in6 addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(MCAST_PORT);

    if (bind(sockfd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
        perror("bind multicast IPv6 failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /*
       Join IPv6 multicast group.

       "lo" is used here so that this works easily
       when testing everything on the same machine.

       For LAN testing, change "lo" to your actual
       network interface, e.g. "eth0" or "wlan0".
    */
    unsigned int ifindex = if_nametoindex("eth0");

    if (ifindex == 0) {
        perror("if_nametoindex failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct ipv6_mreq mreq;

    memset(&mreq, 0, sizeof(mreq));

    inet_pton(AF_INET6,
              MCAST_IPV6,
              &mreq.ipv6mr_multiaddr);

    mreq.ipv6mr_interface = ifindex;

    if (setsockopt(sockfd,
                   IPPROTO_IPV6,
                   IPV6_JOIN_GROUP,
                   &mreq,
                   sizeof(mreq)) < 0) {
        perror("IPv6 multicast join failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

//    Send message to IPv4 multicast group
void sendMulticast4(const char *message, size_t len){
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("multicast IPv4 send socket failed");
        return;
    }

    struct sockaddr_in dest;

    memset(&dest, 0, sizeof(dest));

    dest.sin_family = AF_INET;
    dest.sin_port = htons(MCAST_PORT);

    inet_pton(AF_INET,
              MCAST_IPV4,
              &dest.sin_addr);

    if (sendto(sockfd,
                message,
                len,
                0,
                (struct sockaddr *)&dest,
                sizeof(dest)) < 0) {
        perror("IPv4 multicast send failed");
    }

    close(sockfd);
}


//    Send message to IPv6 multicast group
void sendMulticast6(const char *message, size_t len)
{
    int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("multicast IPv6 send socket failed");
        return;
    }

    struct sockaddr_in6 dest;

    memset(&dest, 0, sizeof(dest));

    dest.sin6_family = AF_INET6;
    dest.sin6_port = htons(MCAST_PORT);

    inet_pton(AF_INET6,
              MCAST_IPV6,
              &dest.sin6_addr);
    //    Send through loopback for local testing.
    dest.sin6_scope_id = if_nametoindex("eth0");

    if (sendto(sockfd,
                message,
                len,
                0,
                (struct sockaddr *)&dest,
                sizeof(dest)) < 0) {
        perror("IPv6 multicast send failed");
    }

    close(sockfd);
}

//    Receive TCP message
ssize_t handleTCPClient(int clientSock)
{
    char buffer[BUFSIZE];

    ssize_t recvLen = recv(
        clientSock,
        buffer,
        BUFSIZE - 1,
        0
    );

    if (recvLen < 0) {
        perror("recv failed");
        return -1;
    }

    if (recvLen == 0) {
        return 0;
    }

    buffer[recvLen] = '\0';

    printTimestamp();
    printf("TCP received: %s", buffer);

    //    Echo back to TCP client.
    ssize_t sentLen = send(
        clientSock,
        buffer,
        recvLen,
        0
    );

    if (sentLen < 0) {
        perror("send failed");
        return -1;
    }

    //    Also send the message to multicast groups.
    sendMulticast4(buffer, recvLen);
    sendMulticast6(buffer, recvLen);

    return recvLen;
}


int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server-port>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    in_port_t servPort = atoi(argv[1]);

    //    TCP sockets
    int tcp4 = createTCP4Socket(servPort);
    int tcp6 = createTCP6Socket(servPort);

    //    Multicast sockets
    int mcast4 = createMulticast4Socket();
    int mcast6 = createMulticast6Socket();

    printTimestamp();
    printf("Server started\n");

    printTimestamp();
    printf("TCP IPv4 listening on port %d\n",
           servPort);

    printTimestamp();
    printf("TCP IPv6 listening on port %d\n",
           servPort);

    printTimestamp();
    printf("IPv4 multicast: %s:%d\n",
           MCAST_IPV4, MCAST_PORT);

    printTimestamp();
    printf("IPv6 multicast: [%s]:%d\n",
           MCAST_IPV6, MCAST_PORT);
    //    Master set.
    fd_set masterSet;

    FD_ZERO(&masterSet);

    FD_SET(STDIN_FILENO, &masterSet);
    FD_SET(tcp4, &masterSet);
    FD_SET(tcp6, &masterSet);
    FD_SET(mcast4, &masterSet);
    FD_SET(mcast6, &masterSet);

    int maxDescriptor = tcp4;

    if (tcp6 > maxDescriptor)
        maxDescriptor = tcp6;

    if (mcast4 > maxDescriptor)
        maxDescriptor = mcast4;

    if (mcast6 > maxDescriptor)
        maxDescriptor = mcast6;

    //    Server loop
    int running = 1;

    while (running) {

        /*
           select() modifies the fd_set.
           Therefore make a copy every time.
        */
        fd_set currentSet;

        memcpy(
            &currentSet,
            &masterSet,
            sizeof(fd_set)
        );

        //    NULL timeout = wait until something happens.
        int activity = select(
            maxDescriptor + 1,
            &currentSet,
            NULL,
            NULL,
            NULL
        );

        if (activity < 0) {
            perror("select failed");
            break;
        }

        for (int fd = 0;
             fd <= maxDescriptor;
             fd++) {

            if (!FD_ISSET(fd, &currentSet))
                continue;

            if (fd == STDIN_FILENO) {

                printTimestamp();
                printf("Shutting down server\n");

                getchar();

                running = 0;
                break;
            }

            //    New IPv4 TCP client
            
            else if (fd == tcp4) {

                int clientSock = acceptClient(tcp4);

                if (clientSock >= 0) {

                    FD_SET(clientSock, &masterSet);

                    if (clientSock > maxDescriptor)
                        maxDescriptor = clientSock;
                }
            }
            //    New IPv6 TCP client
            else if (fd == tcp6) {
                int clientSock = acceptClient(tcp6);
                if (clientSock >= 0) {
                    FD_SET(clientSock, &masterSet);
                    if (clientSock > maxDescriptor)
                        maxDescriptor = clientSock;
                }
            }
            /*
               IPv4 multicast packet
            */
            else if (fd == mcast4) {
                char buffer[BUFSIZE];
                ssize_t recvLen = recvfrom(
                    mcast4,
                    buffer,
                    BUFSIZE - 1,
                    0,
                    NULL,
                    NULL
                );
                if (recvLen > 0) {

                    buffer[recvLen] = '\0';

                    printTimestamp();
                    printf("IPv4 multicast received: %s",
                           buffer);
                }
            }
            /*
               IPv6 multicast packet
            */
            else if (fd == mcast6) {
                char buffer[BUFSIZE];
                ssize_t recvLen = recvfrom(
                    mcast6,
                    buffer,
                    BUFSIZE - 1,
                    0,
                    NULL,
                    NULL
                );
                if (recvLen > 0) {
                    buffer[recvLen] = '\0';
                    printTimestamp();
                    printf("IPv6 multicast received: %s",
                           buffer);
                }
            }
            /*
               Existing TCP client
            */
            else {
                ssize_t recvLen =
                    handleTCPClient(fd);
                if (recvLen <= 0) {
                    printTimestamp();
                    printf("Client disconnected\n");
                    close(fd);
                    FD_CLR(fd, &masterSet);
                }
            }
        }
    }
    /*
       Cleanup
    */
    for (int fd = 0;
         fd <= maxDescriptor;
         fd++) {

        close(fd);
    }
    printTimestamp();
    printf("End of program\n");

    return 0;
}