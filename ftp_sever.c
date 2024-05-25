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

void do_list(int acceptfd);
int do_download(int acceptfd, char *filename);
void do_upload(int acceptfd, char *filename);

int main(int argc, const char *argv[])
{
	int sockfd, acceptfd;
	struct sockaddr_in serveraddr, clientaddr;
	socklen_t addrlen = sizeof(serveraddr);
	char buf[N] = "";

	if(argc < 3){
		fprintf(stderr, "Usage: %s ip port\n", argv[0]);
		return -1;
	}

	/*创建套接字*/
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		errlog("socket error");
	}

	/*填充网络信息结构体
	 *inet_addr:将点分十进制IP地址转换为网络字节序的整型数据
	 *htons:将主机字节序转换为网络字节序
	 *atoi:将数字型字符串转换为整型数据
	 */
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	serveraddr.sin_port = htons(atoi(argv[2]));

	/*将套接字与服务器网络信息结构提绑定*/
	if(bind(sockfd, (struct sockaddr *)&serveraddr, addrlen) < 0){
		errlog("bind error");
	}

	/*将套接字设置为被动监听模式*/
	if(listen(sockfd, 5) < 0){
		errlog("listen error");
	}

	while(1){
		/*阻塞等待客户端的连接请求*/
		if((acceptfd = accept(sockfd,\
					(struct sockaddr *)&clientaddr, &addrlen)) < 0){
			errlog("accept error");
		}

		printf("ip: %s, port = %d\n",\
				inet_ntoa(clientaddr.sin_addr),\
				ntohs(clientaddr.sin_port));

		ssize_t bytes;
		while(1){
			/*接收数据并处理*/
			if((bytes = recv(acceptfd, buf, N, 0)) < 0){
				errlog("recv error");
			}
			/*recv返回值为0,则客户端退出，服务器继续等待新的客户端的连接*/
			else if(bytes == 0){
				printf("client quit\n");
				break;
			}
			else{
				printf("recv data: %s\n", buf);

				switch(buf[0]){
					case 'L':
						/*查看服务器文件*/
						do_list(acceptfd);
						break;
					case 'G':
						/*下载文件*/
						do_download(acceptfd, buf+2);
						break;
					case 'P':
						/*上传文件*/
						do_upload(acceptfd, buf+2);
						break;
				}
			}
		}
	}

	close(acceptfd);
	close(sockfd);
	return 0;
}

void do_list(int acceptfd){
	/*获取当前所有的文件名，并发送给客户端*/
	DIR *dirp;
	struct dirent *dirent;
	char buf[N] = "";

	/*打开目录*/
	if((dirp = opendir(".")) == NULL){
		errlog("opendir error");
	}

	/*读取文件名并发送*/
	while((dirent = readdir(dirp)) != NULL){
		if(dirent->d_name[0] == '.'){
			continue;
		}
		else{
			strcpy(buf, dirent->d_name);
			send(acceptfd, buf, N, 0);
		}
	}
	
	/*发送结束标志，使recv结束循环*/
	strcpy(buf, "**OVER**");
	send(acceptfd, buf, N, 0);

	printf("文件名发送完毕\n");

	return ;
}

int do_download(int acceptfd, char *filename){
	char buf[N] = "";
	int fd;
	ssize_t bytes;

	/*打开文件，判断文件是否存在*/
	if((fd = open(filename, O_RDONLY)) < 0){
		/*如果文件不存在则通知客户端*/
		if(errno == ENOENT){
			strcpy(buf, "NO_EXIST");
			send(acceptfd, buf, N, 0);
			return -1;
		}
		else{
			errlog("open error");
		}
	}

	/*如果文件存在，也需要告诉客户端*/
	strcpy(buf, "YES_EXIST");
	send(acceptfd, buf, N, 0);

	/*读取文件内容并发送*/
	while((bytes = read(fd, buf, N)) > 0){
		send(acceptfd, buf, bytes, 0);
	}

	/*防止数据粘包*/
	sleep(1);
	strcpy(buf, "**OVER**");
	send(acceptfd, buf, N, 0);

	printf("文件发送完毕\n");
	return 0;
}
void do_upload(int acceptfd, char *filename){
	int fd;
	char buf[N] = "";
	ssize_t bytes;

	/*创建文件*/
	if((fd = open(filename,\
					O_RDWR|O_CREAT|O_TRUNC, 0664)) < 0){
		errlog("open error");
	}

	/*接收内容并写入文件*/
	while((bytes = recv(acceptfd, buf, N, 0)) > 0){
		if(strncmp(buf, "**OVER**", 8) == 0){
			break;
		}
		write(fd, buf, bytes);
	}

	printf("文件接收完毕\n");
	return ;
}

