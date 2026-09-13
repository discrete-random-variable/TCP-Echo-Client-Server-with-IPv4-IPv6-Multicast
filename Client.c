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

#define MCAST_PORT 5000
#define MCAST_IPV4 "239.1.1.1"
#define MCAST_IPV6 "ff02::1234"

/* ---------------------------------------------------------
   Timestamp
   --------------------------------------------------------- */
void printTimestamp(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    printf("[%02d:%02d:%02d] ",
           t->tm_hour,
           t->tm_min,
           t->tm_sec);
}

/* ---------------------------------------------------------
   IPv4 multicast receiver
   --------------------------------------------------------- */
int createMulticast4Socket(void)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("IPv4 multicast socket failed");
        exit(EXIT_FAILURE);
    }

    int yes = 1;

    setsockopt(sockfd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &yes,
               sizeof(yes));

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(MCAST_PORT);

    if (bind(sockfd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {

        perror("IPv4 multicast bind failed");
        exit(EXIT_FAILURE);
    }

    /*
       Join 239.1.1.1
    */
    struct ip_mreq mreq;

    memset(&mreq, 0, sizeof(mreq));
    inet_pton(AF_INET,
              MCAST_IPV4,
              &mreq.imr_multiaddr);

    mreq.imr_interface.s_addr =
        htonl(INADDR_ANY);

    if (setsockopt(sockfd,
                   IPPROTO_IP,
                   IP_ADD_MEMBERSHIP,
                   &mreq,
                   sizeof(mreq)) < 0) {

        perror("IPv4 multicast join failed");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}
/* ---------------------------------------------------------
   IPv6 multicast receiver
   --------------------------------------------------------- */
int createMulticast6Socket(void)
{
    int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("IPv6 multicast socket failed");
        exit(EXIT_FAILURE);
    }

    int yes = 1;

    setsockopt(sockfd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &yes,
               sizeof(yes));

    struct sockaddr_in6 addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(MCAST_PORT);

    if (bind(sockfd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {

        perror("IPv6 multicast bind failed");
        exit(EXIT_FAILURE);
    }
    /*
       Join IPv6 multicast group on loopback.
    */
    unsigned int ifindex = if_nametoindex("eth0");

    if (ifindex == 0) {
        perror("if_nametoindex failed");
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
        exit(EXIT_FAILURE);
    }

    return sockfd;
}


/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */
int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr,
                "Usage: %s <server-hostname> <server-port>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }

    char *serverName = argv[1];
    char *serverPort = argv[2];

    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *rp;

    memset(&hints, 0, sizeof(hints));

    /*
       AF_UNSPEC = IPv4 OR IPv6
    */
    hints.ai_family = AF_UNSPEC;

    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;


    int err = getaddrinfo(
        serverName,
        serverPort,
        &hints,
        &result
    );

    if (err != 0) {

        fprintf(stderr,
                "getaddrinfo() failed: %s\n",
                gai_strerror(err));

        exit(EXIT_FAILURE);
    }
    /*
       ------------------------------------------------------
       Try every address returned by getaddrinfo()
       ------------------------------------------------------
    */
    int sockfd = -1;

    for (rp = result;
         rp != NULL;
         rp = rp->ai_next) {
        /*
           Create socket according to returned address.
        */
        sockfd = socket(
            rp->ai_family,
            rp->ai_socktype,
            rp->ai_protocol
        );
        if (sockfd < 0)
            continue;
        /*
           Try to connect.
        */
        if (connect(
                sockfd,
                rp->ai_addr,
                rp->ai_addrlen
            ) == 0) {

            /*
               Connection successful.
            */
            break;
        }

        close(sockfd);
        sockfd = -1;
    }


    if (sockfd < 0) {
        fprintf(stderr,
                "Could not connect to server\n");
        freeaddrinfo(result);
        exit(EXIT_FAILURE);
    }
    /*
       We no longer need the address list.
    */
    freeaddrinfo(result);

    printTimestamp();
    printf("Connected to %s:%s\n",
           serverName,
           serverPort);

    /*
       ------------------------------------------------------
       Create multicast sockets
       ------------------------------------------------------
    */
    int mcast4 = createMulticast4Socket();
    int mcast6 = createMulticast6Socket();
    /*
       ------------------------------------------------------
       select() setup
       ------------------------------------------------------
    */
    fd_set masterSet;

    FD_ZERO(&masterSet);

    FD_SET(STDIN_FILENO, &masterSet);
    FD_SET(sockfd, &masterSet);
    FD_SET(mcast4, &masterSet);
    FD_SET(mcast6, &masterSet);


    int maxDescriptor = sockfd;

    if (mcast4 > maxDescriptor)
        maxDescriptor = mcast4;

    if (mcast6 > maxDescriptor)
        maxDescriptor = mcast6;
    /*
       ------------------------------------------------------
       Client loop
       ------------------------------------------------------
    */
    int running = 1;
    struct timespec start, end;
    while (running) {

        fd_set currentSet;
        memcpy(
            &currentSet,
            &masterSet,
            sizeof(fd_set)
        );
        /*
           Wait until:
           - keyboard input
           - TCP data
           - IPv4 multicast
           - IPv6 multicast
        */
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

        if (FD_ISSET(STDIN_FILENO, &currentSet)) {
            char message[BUFSIZE];
            printf("Type string: ");
            if (fgets(message,
                      BUFSIZE,
                      stdin) == NULL) {
                break;
            }
            if (strncmp(message, "BYE", 3) == 0) {
                running = 0;
                break;
            }
            size_t messageLen = strlen(message);
            clock_gettime(CLOCK_MONOTONIC, &start);

            ssize_t sentLen = send(
                sockfd,
                message,
                messageLen,
                0
            );

            if (sentLen < 0) {
                perror("send failed");
                break;
            }

            printTimestamp();
            printf("TCP sent: %s", message);
        }
        /*
           TCP response
        */

        if (FD_ISSET(sockfd, &currentSet)) {
            char buffer[BUFSIZE];
            ssize_t recvLen = recv(
                sockfd,
                buffer,
                BUFSIZE - 1,
                0
            );
            
            if (recvLen < 0) {
                perror("recv failed");
                break;
            }
            clock_gettime(CLOCK_MONOTONIC, &end);

            if (recvLen == 0) {
                printTimestamp();
                printf("Server disconnected\n");
                break;
            }
            long seconds = end.tv_sec - start.tv_sec;
            long nanoseconds = end.tv_nsec - start.tv_nsec;

            double rtt = seconds * 1000.0 +
                        nanoseconds / 1000000.0;
            buffer[recvLen] = '\0';

            printTimestamp();
            printf("TCP received: %s", buffer);
            printf("RTT: %.3f ms\n", rtt);
        }


        /*
           IPv4 multicast
        */

        if (FD_ISSET(mcast4, &currentSet)) {

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
                printf("MULTICAST IPv4: %s",
                       buffer);
            }
        }


        /*
           IPv6 multicast
        */

        if (FD_ISSET(mcast6, &currentSet)) {
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
                printf("MULTICAST IPv6: %s",
                       buffer);
            }
        }
    }


    close(sockfd);
    close(mcast4);
    close(mcast6);

    printTimestamp();
    printf("Client terminated\n");

    return 0;
}