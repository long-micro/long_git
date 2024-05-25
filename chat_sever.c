#include <stdio.h> 
#include <arpa/inet.h> 
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h>

#define N 128
#define errlog(errmsg) do{perror(errmsg);\
						  printf("%s--%s--%d\n",\ 
								  __FILE__, __func__, __LINE__);\
	  					  exit(1);\
	     				 }while(0)
#define L 1 //login
#define C 2 //chat
#define Q 3 //quit

//注意:服务器和客户端如果通信的结构体一样,必须保证成员顺序一样
typedef struct{
	int type;
	char name[N];
	char text[N];
}MSG;

//创建链表结构体,保存用户信息
typedef struct node{
	struct sockaddr_in addr;
	struct node *next;
}linklist_t;

linklist_t *linklist_create();
void do_login(MSG msg, linklist_t *h, 
		int sockfd, struct sockaddr_in clientaddr);
void do_chat(MSG msg, linklist_t *h, 
		int sockfd, struct sockaddr_in clientaddr);
void do_quit(MSG msg, linklist_t *h, 
		int sockfd, struct sockaddr_in clientaddr);

int main(int argc, const char *argv[])
{
	int sockfd;
	struct sockaddr_in serveraddr, clientaddr;
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
	//inet_addr:将点分十进制ip地址转化为网络字节序整型数据
	//htons:将主机字节序转化为网络字节序
	//atoi:将数字形字符串转化为整型数据
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	serveraddr.sin_port = htons(atoi(argv[2]));

	//第三步:将套接字与服务器网络信息结构体绑定
	if(bind(sockfd, (struct sockaddr *)&serveraddr, addrlen) < 0){
		errlog("fail to bind");
	}

	MSG msg;

	//创建父子进程,实现一边发送系统信息,一边接收数据并处理
	pid_t pid;
	if((pid = fork()) < 0){
		errlog("fail to fork");
	}
	else if(pid == 0){ //子进程负责发送系统信息
		msg.type = C;
		strcpy(msg.name, "server");

		while(1){
			fgets(msg.text, N, stdin);
			msg.text[strlen(msg.text) - 1] = '\0';

			sendto(sockfd, &msg, sizeof(msg), 0,\
					(struct sockaddr*)&serveraddr, addrlen);
		}
	}
	else{ //父进程负责接收数据并处理
		linklist_t *h = linklist_create();

		while(1){
			recvfrom(sockfd, &msg, sizeof(msg), 0,\ 
					(struct sockaddr *)&clientaddr, &addrlen);

			printf("%d -- %s -- %s\n", msg.type, msg.name, msg.text);

			//根据接收的数据中的类型分别进行处理
			switch(msg.type){
				case L:
					do_login(msg, h, sockfd, clientaddr);
					break;
				case C:
					do_chat(msg, h, sockfd, clientaddr);
					break;
				case Q:
					do_quit(msg, h, sockfd, clientaddr);
					break;
			}
		}
	}
	return 0;
}
//创建一个空的链表
linklist_t *linklist_create(){
	linklist_t *h = (linklist_t *)malloc(sizeof(linklist_t));

	h->next = NULL;

	return h;
}

void do_login(MSG msg, linklist_t *h, int sockfd,\ 
		                      struct sockaddr_in clientaddr)
{
	//将用户登陆的信息发送给其他在线的用户
	sprintf(msg.text, "-------- %s login ------------", msg.name);
	//遍历链表并发送数据
	while(h->next != NULL){
		sendto(sockfd, &msg, sizeof(msg), 0,\
		 (struct sockaddr *)&h->next->addr, sizeof(struct sockaddr_in));

		h = h->next;
	}
	//将新登录的用户的信息保存在链表里面
	linklist_t *temp = (linklist_t *)malloc(sizeof(linklist_t));
	
	temp->addr = clientaddr;
	// temp->next = h->next;
	// h->next = temp;
	linklist_t *p_mov = h;
	while (p_mov->next != NULL) {
		p_mov = p_mov->next;
	}
	p_mov->next = temp;

	return ;
}

void do_chat(MSG msg, linklist_t *h,\ 
		     int sockfd, struct sockaddr_in clientaddr)
{
	char buf[N] = {};
	linklist_t *p = h;

	//将用户发送的信息发送给其他在线的用户
	sprintf(buf, "%s : %s", msg.name, msg.text);
	strcpy(msg.text, buf);
	
	//发送数据
	while(p->next != NULL){
		//自己不接收自己发送的数据
		if(memcmp(&clientaddr, &p->next->addr,\ 
					 sizeof(clientaddr)) == 0){
			p = p->next;
		}
		else{
			sendto(sockfd, &msg, sizeof(msg), 0,\ 
			       (struct sockaddr *)&p->next->addr,\ 
				     sizeof(struct sockaddr_in));
			p = p->next;
		}
	}

	return ;
}
void do_quit(MSG msg, linklist_t *h,\
		     int sockfd, struct sockaddr_in clientaddr)
{
	linklist_t *p = h;
	linklist_t *temp;

	//将用户退出的信息发送给其他用户并将其信息从链表删除

	sprintf(msg.text, "-------- %s offline --------", msg.name);

	while(p->next != NULL){
	//如果找到自己的信息,则将其从链表删除
		if(memcmp(&clientaddr, &p->next->addr,\ 
					 sizeof(clientaddr)) == 0){
			temp = p->next;
			p->next = temp->next;
	
			free(temp);
			temp = NULL;
		}
		else{
			sendto(sockfd, &msg, sizeof(msg), 0,\ 
				(struct sockaddr *)&p->next->addr,\ 
				  sizeof(struct sockaddr_in));

			p = p->next;
		}
	}
	
	return ;
}
	
