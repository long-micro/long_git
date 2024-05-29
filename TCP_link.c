#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/if_ether.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#define ack_syn 0x12

struct ip_msg {
    char *addr_src;
    char *addr_dst;
    int port_src;
    int port_dst;
};

static void iphead_init(struct iphdr *iphead,struct ip_msg ip_msg)
{
    iphead->version = 4;
    iphead->ihl = 5;
    iphead->tos = 0;
    iphead->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    iphead->id = htons(1234);
    iphead->frag_off = 0;
    iphead->ttl = 64;
    iphead->protocol = IPPROTO_TCP;
    iphead->saddr = inet_addr(ip_msg.addr_src);
    iphead->daddr = inet_addr(ip_msg.addr_dst);
    iphead->check = 0;
}

static void tcp_init(struct tcphdr *tcphead, struct ip_msg ip_msg)
{
    tcphead->source = htons(ip_msg.port_src);
    tcphead->dest = htons(ip_msg.port_dst);
    tcphead->seq = htonl(1);
    tcphead->ack_seq= htonl(0);
    tcphead->doff = sizeof(struct tcphdr) / 4;
    tcphead->window = htons(4096); // 窗口大小
    tcphead->check = 0;
}

static void communicate_init(struct sockaddr_in *server_addr, struct ip_msg ip_msg, struct iphdr *iphead, struct tcphdr *tcphead)
{
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = inet_addr(ip_msg.addr_dst);
    server_addr->sin_port = htons(ip_msg.port_dst);

    iphead_init(iphead, ip_msg);
    tcp_init(tcphead, ip_msg);
}

static inline uint8_t get_protocol_type(uint8_t *packet)
{
    return *(packet + 9);
}

static inline uint8_t get_ctl_type(uint8_t *packet)
{
    return *(packet + 33) & 0x3F;
}

static void syn_msg(struct tcphdr *tcphead)
{
    tcphead->syn = 1;
    tcphead->ack = 0;
    tcphead->fin = 0;
    tcphead->rst = 0;
    tcphead->psh = 0;
    tcphead->urg = 0;
    tcphead->seq = htonl(0);
    tcphead->ack_seq= htonl(0);
}

static void ack_msg(struct tcphdr *tcphead, uint8_t *recv_packet)
{   
    uint32_t recv_seq = *((uint32_t *)recv_packet + 6);

    tcphead->syn = 0;
    tcphead->ack = 1;
    tcphead->fin = 0;
    tcphead->rst = 0;
    tcphead->psh = 0;
    tcphead->urg = 0;
    tcphead->seq = htonl(1);
    tcphead->ack_seq= htonl(htonl(recv_seq) + 1);
}

static bool check_msg(uint8_t *recv_packet)
{
    uint8_t protocol_type = get_protocol_type(recv_packet);
    if (IPPROTO_TCP == protocol_type) {
        uint8_t ctl_type = get_ctl_type(recv_packet);
        if (ack_syn == ctl_type) {
            return true;
        }
    }
    
    return false;
}

static int socket_init()
{
    int sockfd = 0;
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        printf("socket error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    int optval = 1;
    int seted = setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(optval));
    if (seted < 0) {
        (void)close(sockfd);
        printf("setsockopt error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    return sockfd;
}

int main (int argv, char *argc[])
{
    int sockfd = 0;
    uint8_t send_packet[1024] = {0};
    uint8_t recv_packet[1024] = {0};
    struct iphdr *iphead = (struct iphdr *)send_packet;
    struct tcphdr *tcphead = (struct tcphdr *)(send_packet + sizeof(struct iphdr));
    struct sockaddr_in server_addr = {0};

    sockfd = socket_init();

    struct ip_msg ip_msg = {"172.31.159.18", "39.156.66.18", 7777, 80};
    communicate_init(&server_addr, ip_msg, iphead, tcphead);

    syn_msg(tcphead);
    int sended_syn = sendto(sockfd, send_packet, iphead->tot_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sended_syn < 0) {
        (void)close(sockfd);
        printf("send error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    for (bool checked = check_msg(recv_packet); true != checked; checked = check_msg(recv_packet)) {
        int recved = recvfrom(sockfd, recv_packet, iphead->tot_len, 0, NULL, NULL);
        if (recved < 0) {
            (void)close(sockfd);
            printf("send error, err(%d:%s)\n", errno, strerror(errno));
            return -1;
        }
    }

    ack_msg(tcphead, recv_packet);
    int sended_ack = sendto(sockfd, send_packet, iphead->tot_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sended_ack < 0) {
        (void)close(sockfd);
        printf("send error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    (void)close(sockfd);

    return 0;
}