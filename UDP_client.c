#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

static int client_init(const char *ip_addr, const int ip_port, struct sockaddr_in *addr_server)
{
    int sockfd = 0;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("socket error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    addr_server->sin_family = AF_INET;
    addr_server->sin_addr.s_addr = inet_addr(ip_addr);
    addr_server->sin_port = htons(ip_port);

    return sockfd;
}

static void send_msg(int sockfd, struct sockaddr_in addr_server)
{
    while (1) {
        char buff[1024] = {0};

        (void)fgets(buff, sizeof(buff), stdin);
        buff[strlen(buff) - 1] = '\0';

        int sended = sendto(sockfd, buff, sizeof(buff), 0, (struct sockaddr *)&addr_server, sizeof(addr_server));
        if (sended < 0) {
            printf("send error, err(%d:%s)\n", errno, strerror(errno));
            continue;
        }

        int quited = strncmp(buff, "quit", 4);
        if (0 == quited) {
            printf("client quit\n");
            break;
        }
    }    
}

int main (int argv, char *argc[])
{
    int sockfd = 0;
    struct sockaddr_in addr_server = {0};

    sockfd = client_init("172.31.159.18", 7777, &addr_server);

    send_msg(sockfd, addr_server);

    (void)close(sockfd);

    return 0;
}