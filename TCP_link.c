#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/if_ether.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#define ack_syn 0x12

static void iphead_init(struct iphdr *iphead,const char *ip_src, const char *ip_dst)
{
    iphead->version = 4;
    iphead->ihl = 5;
    iphead->tos = 0;
    iphead->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    iphead->id = htons(1234);
    iphead->frag_off = 0;
    iphead->ttl = 64;
    iphead->protocol = IPPROTO_TCP;
    iphead->saddr = inet_addr(ip_src);
    iphead->daddr = inet_addr(ip_dst);
    iphead->check = 0;
}

static void tcp_init(struct tcphdr *tcphead, int port_src, int port_dst)
{
    tcphead->source = htons(port_src);
    tcphead->dest = htons(port_dst);
    tcphead->seq = htonl(1);
    tcphead->ack_seq= htonl(0);
    tcphead->doff = sizeof(struct tcphdr) / 4;
    tcphead->window = htons(4096); // 窗口大小
    tcphead->check = 0;
}

static void communicate_init(struct sockaddr_in *server_addr, const char *ip_dst, int port_dst)
{
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = inet_addr(ip_dst);
    server_addr->sin_port = htons(port_dst);
}

static uint8_t get_protocol_type(uint8_t *packet)
{
    uint8_t protocol_type = *(packet + 9);

    return protocol_type;
}

static uint8_t get_ctl_type(uint8_t *packet)
{
    uint8_t ctl_type = *(packet + 33) & 0x3F;

    return ctl_type;
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

int main (int argv, char *argc[])
{
    int sockfd = 0;
    uint8_t send_packet[1024] = {0};
    uint8_t recv_packet[1024] = {0};
    struct iphdr *iphead = (struct iphdr *)send_packet;
    struct tcphdr *tcphead = (struct tcphdr *)(send_packet + sizeof(struct iphdr));
    struct sockaddr_in server_addr = {0};

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sockfd < 0) {
        printf("socket error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    int optval = 1;
    int seted = setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(optval));
    if (seted < 0) {
        close(sockfd);
        printf("setsockopt error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    iphead_init(iphead, "172.31.159.18", "39.156.66.18");
    tcp_init(tcphead, 7777, 80);
    communicate_init(&server_addr, "39.156.66.18", 80);

    syn_msg(tcphead);
    int sended_ack = sendto(sockfd, send_packet, iphead->tot_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (sended_ack < 0) {
        close(sockfd);
        printf("send error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    while (1) {
        recvfrom(sockfd, recv_packet, iphead->tot_len, 0, NULL, NULL);
        uint8_t protocol_type = get_protocol_type(recv_packet);
        if (IPPROTO_TCP == protocol_type) {
            uint8_t ctl_type = get_ctl_type(recv_packet);
            if (ack_syn == ctl_type) {
                ack_msg(tcphead, recv_packet);
                int sended_msg = sendto(sockfd, send_packet, iphead->tot_len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                if (sended_msg < 0) {
                    close(sockfd);
                    printf("send error, err(%d:%s)\n", errno, strerror(errno));
                    return -1;
                }
                break;
            }
            else {
                continue;
            }
        }
        else {
            continue;
        }
    }

    close(sockfd);

    return 0;
}