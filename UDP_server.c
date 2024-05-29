#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

static int server_init(const char *ip_addr, int ip_port)
{   
    int sockfd = 0;
    struct sockaddr_in addr_server = {0};

    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = inet_addr(ip_addr);
    addr_server.sin_port = htons(ip_port);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("socket error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    socklen_t addr_len = sizeof(struct sockaddr_in);
    int binded = bind(sockfd, (struct sockaddr *)&addr_server, addr_len);
    if (binded < 0) {
        (void)close(sockfd);
        printf("bind error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    return sockfd;
}

static void recv_msg(int sockfd)
{
    while (1) {
        char buff[1024] = {0};

        size_t bytes = recvfrom(sockfd, buff, sizeof(buff), 0, NULL, NULL);
        if (bytes < 0) {
            printf("recvfrom error, err(%d:%s)\n", errno, strerror(errno));
            continue;
        }
        else {
            int quited = strcmp(buff, "quit");
            if (0 == quited){
                printf("sever quit\n");
                break;
            }
            else{
                printf("%s\n", buff);
            }
        }
    }
}

int main (int agv, char *argc[])
{
    int sockfd = 0;
    
    sockfd = server_init("172.31.159.18", 7777);
    recv_msg(sockfd);

    (void)close(sockfd);
    return 0;
}