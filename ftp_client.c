#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define N 128
#define errlog(errmsg) do{perror(errmsg);\
						  printf("%s--%s--%d\n",\
								  __FILE__, __func__, __LINE__);\
						  exit(1);\
						 }while(0)
void do_help();
void do_list(int sockfd);
int do_download(int sockfd, char *filename);
int do_upload(int sockfd, char *filename);

int main(int argc, const char *argv[])
{
	int sockfd;
	struct sockaddr_in serveraddr;
	socklen_t addrlen = sizeof(serveraddr);
	char buf[N] = "";

	if(argc < 3){
		fprintf(stderr, "Usage:%s ip port\n", argv[0]);
		return -1;
	}
	/*创建套接字*/
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		errlog("socket error");
	}

	/*填充服务器网络信息结构体
	 *inet_addr:将点分十进制IP地址转换为网络字节序的整型数据
	 *htons:将主机字节序转换为网络字节序
	 *atoi:将数字型字符串转换为整型数据
	 */
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	serveraddr.sin_port = htons(atoi(argv[2]));

#if 0
	系统可以随机为客户端指定IP地址和端口号，客户端也可以自行设定
	struct sockaddr_in clientaddr;
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_addr.s_addr = inet_addr(argv[3]);
	clientaddr.sin_port = htons(atoi(argv[4]));
#endif

	/*发送客户端连接请求*/
	if(connect(sockfd, (struct sockaddr *)&serveraddr, addrlen) < 0){
		errlog("connect error");
	}

	printf("************************************\n");
	printf("*********请输入help查看选项*********\n");
	printf("************************************\n");

	while(1){
		printf(">>> ");
		/*输入数据并做出相应的判断*/
		fgets(buf, N, stdin);
		buf[strlen(buf) - 1] = '\0';

		/*判断输入的内容*/
		if(strncmp(buf, "help", 4) == 0){
			do_help();
		}
		else if(strncmp(buf, "list", 4) == 0){
			do_list(sockfd);
		}
		else if(strncmp(buf, "get", 3) == 0){
			do_download(sockfd, buf+4);
		}
		else if(strncmp(buf, "put", 3) == 0){
			do_upload(sockfd, buf+4);
		}
		else if(strncmp(buf, "quit", 4) == 0){
			printf("client quit\n");
			close(sockfd);
			return -1;
		}
		else{
			printf("输入有误，请重新输入！！！\n");
		}
	}
	return 0;
}

void do_help(){
	printf("*************************************************\n");
	printf("***         输入  /  功能 ***********************\n");
	printf("***         list  /  查看服务器所在目录的文件名 *\n");
	printf("*** get filename  /  下载服务器所在目录的文件 ***\n");
	printf("*** put filename  /  上传文件到服务器 ***********\n");
	printf("***         quit  /  退出 ***********************\n");
	printf("*************************************************\n");

	return ;
}

void do_list(int sockfd){
	char buf[N] = "";

	/*发送指令报告服务器执行查看目录文件名的功能*/
	strcpy(buf, "L");
	send(sockfd, buf, N, 0);

	/*接收文件名并打印*/
	while(recv(sockfd, buf, N, 0) > 0){
		if(strncmp(buf, "**OVER**", 8) == 0){
			break;
		}
		printf("%s\n", buf);
	}

	printf("文件名接收完毕\n");

	return ;
}

int do_download(int sockfd, char *filename){
	/*发送指令以及文件名告知服务器执行下载的功能*/
	char buf[N] = "";
	int fd;
	ssize_t bytes;

	sprintf(buf, "G %s", filename);
	send(sockfd, buf, N, 0);
	
	/*接收数据，获取文件是否存在的信息*/
	recv(sockfd, buf, N, 0);

	/*如果文件不存在，则退出函数*/
	if(strncmp(buf, "NO_EXIST", 8) == 0){
		printf("文件%s不存在，请重新输入！！！\n", filename);
		return -1;
	}

	/*如果文件存在，创建文件*/
	if((fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0664)) < 0){
		errlog("open error");
	}

	/*接收内容并写入文件*/
	while((bytes = recv(sockfd, buf, N, 0)) > 0){
		if(strncmp(buf, "**OVER**", 8) == 0){
			break;
		}
		write(fd, buf, bytes);
	}

	printf("文件下载完毕\n");
	return 0;
}

int do_upload(int sockfd, char *filename){
	int fd;
	char buf[N] = "";
	ssize_t bytes;

	/*打开文件，判断文件是否存在*/
	if((fd = open(filename, O_RDONLY)) < 0){
		if(errno == ENOENT){
			printf("文件%s不存在，请重新输入\n", filename);
			return -1;
		}
		else{
			errlog("open error");
		}
	}

	/*如果文件存在，将指令以及文件名发送给服务器*/
	sprintf(buf, "P %s", filename);
	send(sockfd, buf, N, 0);

	/*读取文件内容并发送*/
	while((bytes = read(fd, buf, N)) > 0){
		send(sockfd, buf, bytes, 0);
	}

	/*发送结束标志*/
	sleep(1);
	strcpy(buf, "**OVER**");
	send(sockfd, buf, N, 0);

	printf("文件上传完毕\n");

	return 0;
}
