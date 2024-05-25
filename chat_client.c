#include <stdio.h> 
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h>
#include <strings.h>
#include <signal.h>

#define N 128
#define errlog(errmsg) do{perror(errmsg);\
	  					  printf("%s--%s--%d\n",\ 
								  __FILE__, __func__, __LINE__);\
	  				      exit(1);\
	     				 }while(0)

#define L 1 //login
#define C 2 //chat
#define Q 3 //quit

typedef struct{
	int type;
	char name[N];
	char text[N];
}MSG;

int main(int argc, const char *argv[])
{
	int sockfd;
	struct sockaddr_in serveraddr;
	socklen_t addrlen = sizeof(serveraddr);

	if(argc < 3){
		fprintf(stderr, "Usage: %s ip port\n", argv[0]);
		exit(1);
	}

	//第一步:创建套接字
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		errlog("fail to socket");
	}

	//第二步:填充服务器网络信息结构体
	//inet_addr:将点分十进制ip地址转化为网络字节序的整型数据
	//htons:将主机字节序转化为网络字节序
	//atoi:将数字形字符串转化为整型数据
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	serveraddr.sin_port = htons(atoi(argv[2]));

	MSG msg;
	bzero(&msg, sizeof(msg));

	//登录操作
	//将指令以及用户名发送给服务器
	msg.type = L;
	
	printf("请输入您的姓名: ");
	fgets(msg.name, N, stdin);
	msg.name[strlen(msg.name) - 1] = '\0';

	sendto(sockfd, &msg, sizeof(msg), 0,\ 
			(struct sockaddr *)&serveraddr, addrlen);

	//创建父子进程,实现一边发送数据,一边接收数据
	pid_t pid;

	if((pid = fork()) < 0){
		errlog("fail to fork");
	}
	else if(pid == 0){ //子进程负责发送数据
		while(1){
			fgets(msg.text, N, stdin);
			msg.text[strlen(msg.text) - 1] = '\0';

			//退出信息
			if(strncmp(msg.text, "quit", 4) == 0){
				msg.type = Q;
				sendto(sockfd, &msg, sizeof(msg), 0,\ 
						(struct sockaddr *)&serveraddr, addrlen);
				close(sockfd);
				kill(getppid(), SIGKILL);
				exit(1);
			}
			//群聊信息
			msg.type = C;
			sendto(sockfd, &msg, sizeof(msg), 0,\ 
					(struct sockaddr *)&serveraddr, addrlen);
		}
	}
	else{ //父进程负责接收数据
		//服务器将数据保存在MSG结构体的text里面
		while(1){
			recvfrom(sockfd, &msg, sizeof(msg), 0,\ 
					  (struct sockaddr *)&serveraddr, &addrlen);

			printf("%s\n", msg.text);
		}
	}
	
	return 0;
}
