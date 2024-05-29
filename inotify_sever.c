#include <arpa/inet.h>
#include <errno.h>
#include <ftw.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))        //1024个事件长度,+16是经验值，因为结构体中name[]是可变长度的，+16是用来预留文件名存储的

struct msg_data {
    char is_data[32];
    char data[1024];
};

struct file_msg {
    char fpath[1024];
    char ftype[32];
    char action[32];
    struct msg_data fdata;
};

int *sockfd = NULL;
int fd = 0; 
char **dir_storage = NULL;
unsigned int dir_max = 4;
unsigned int dir_index = 0;
struct file_msg allfile_msg = {0};

static void dir_push(char *fpath)
{
    if ((dir_index + 1) > (dir_max - 1)) {       //防止开辟空间不够用 
        dir_max = dir_index + 2;
        dir_storage = realloc(dir_storage, dir_max * sizeof(char *));
        if (NULL == dir_storage) {
            printf("realloc error, err(%d:%s)\n", errno, strerror(errno));
            return;
        }            
    }

    dir_storage[dir_index++] = strdup(fpath);      //存储目录绝对地址 
}

static void create_msg(struct file_msg *inotify_msg) 
{
    struct stat file_tpye = {0};        //获取文件类型等信息
    (void)lstat(inotify_msg->fpath, &file_tpye);

    strncpy(inotify_msg->action, "create", 6);

    if (1 == S_ISDIR(file_tpye.st_mode)) {
        strncpy(inotify_msg->ftype, "dir", 3);
        
        inotify_add_watch(fd, inotify_msg->fpath, IN_DELETE | IN_CREATE | IN_MODIFY | IN_MOVE);
        dir_push(inotify_msg->fpath);
    }
    else if (1 == S_ISLNK(file_tpye.st_mode)) {
        strncpy(inotify_msg->ftype, "link", 4);
        (void)readlink(inotify_msg->fpath, inotify_msg->fdata.data, sizeof(inotify_msg->fdata.data));    
    }
    else {
        strncpy(inotify_msg->ftype, "regular", 7);
    }
}

static void delete_msg(struct file_msg *inotify_msg) 
{
    struct stat file_tpye = {0};        //获取文件类型等信息
    (void)lstat(inotify_msg->fpath, &file_tpye);

    strncpy(inotify_msg->action, "delete", 6);

    if (1 == S_ISDIR(file_tpye.st_mode)) {
        strncpy(inotify_msg->ftype, "dir", 3);
    }
    else {
        strncpy(inotify_msg->ftype, "regular", 6);
    }
}

static void modify_msg(struct file_msg *inotify_msg) 
{
    struct stat file_tpye = {0};        //获取文件类型等信息
    (void)lstat(inotify_msg->fpath, &file_tpye);

    strncpy(inotify_msg->action, "modify", 6); 

    if (1 == S_ISREG(file_tpye.st_mode)){
        strncpy(inotify_msg->ftype, "regular", 7);
    }
}

static void *sever_link(const char *ip_addr, int ip_port)
{
    /*连接*/
    static int sockfd[2] = {0};
    struct sockaddr_in addr = {0};
    socklen_t addrlen = sizeof(addr);

    addr.sin_family = AF_INET;         //IPV4标识符
    addr.sin_addr.s_addr = inet_addr(ip_addr);      //十进制点分IP地址转换为网络字节序
    addr.sin_port = htons(ip_port);       //主机字节序转化为网络字节序

    sockfd[0] = socket(AF_INET, SOCK_STREAM, 0);        //打开套接字
    if (sockfd[0] < 0) {
        printf("socket error, err(%d:%s)\n", errno, strerror(errno));
        return;
    }

    int binded = bind(sockfd[0], (struct sockaddr *)&addr, addrlen);        //绑定ip地址和端口
    if (binded < 0) {
        (void)close(sockfd[0]);
        printf("bind error, err(%d:%s)\n", errno, strerror(errno));
        return;
    }

    int listened = listen(sockfd[0], 5);     //将一个套接字从主动连接模式转换为被动监听模式
	if (listened < 0) {    
        (void)close(sockfd[0]);  
		printf("listen error, err(%d:%s)\n", errno, strerror(errno));
        return;
	}
    
    sockfd[1] = accept(sockfd[0], NULL, NULL);      //阻塞等待客户端的连接请求,后面两个参数设置为NULL表示不关注客户端发送的信息，不影响连接
    if (sockfd[1] < 0) {
        (void)close(sockfd[0]);
        printf("accept error, err(%d:%s)\n", errno, strerror(errno));
        return;
    }

    return &sockfd;
}

static void sever_send(int sockfd, void *inotify_msg)
{
    int sended = send(sockfd, (char *)inotify_msg, sizeof(struct file_msg), 0);
    if (sended < 0) {
        (void)close(sockfd);
        printf("send error, err(%d:%s)\n", errno, strerror(errno));
        return;
    }
}

static void send_file_data(struct file_msg fmsg)
{
    FILE *file =  NULL;

    file = fopen(fmsg.fpath, "r");   //打开文件
    if (NULL == file) {
        printf("fopen error, err(%d:%s)\n", errno, strerror(errno));
        return;
    }
    
    while(1) {    //读文件
        int rd_num = 0;

        rd_num = fread(fmsg.fdata.data, sizeof(char), sizeof(fmsg.fdata.data), file);
        if (0 != ferror(file)) {
            (void)fclose(file);
            printf("fread error, err(%d:%s)\n", errno, strerror(errno));
            return;
        }

        strncpy(fmsg.fdata.is_data, "yes", 3);;
        sever_send(sockfd[1], &fmsg);       //发送到客户端
        memset(fmsg.fdata.data, 0, sizeof(fmsg.fdata.data));
        memset(fmsg.fdata.is_data, 0, sizeof(fmsg.fdata.is_data));

        if (rd_num < sizeof(fmsg.fdata.data)) {
            break;
        }
    }

    (void)fclose(file);   //关闭文件
}

static void sever_close(int *sockfd)
{
    close(sockfd[0]);
    close(sockfd[1]);
}

static struct file_msg *getmsg(struct inotify_event *event)       
{
    static struct file_msg inotify_msg = {0};

    char absolute_path[255] = {0};
    (void)sprintf(absolute_path, "%s/%s", dir_storage[event->wd-1], event->name);     //变化文件绝对路径
    strcpy(inotify_msg.fpath, absolute_path);

	if (event->mask & IN_CREATE) {      //创建
       create_msg(&inotify_msg);
    } 
	else if (event->mask & IN_DELETE) {     //删除
       delete_msg(&inotify_msg);
	}
    else if (event->mask & IN_MODIFY) {     //修改
       modify_msg(&inotify_msg);
    }

    printf("%s %s %s\n", inotify_msg.fpath, inotify_msg.ftype, inotify_msg.action);

    return &inotify_msg;
}

static void event_read()
{   
    int length = 0;
    char buff[EVENT_BUF_LEN] = {0};
    struct file_msg *inotify_msg = NULL;

    length = read(fd, buff, EVENT_BUF_LEN);     //读取事件总长度
    if (length < 0) {
        (void)close(fd);
        printf("read error, err(%d:%s)\n", errno, strerror(errno));
        return;
    }

    int i = 0;
    while (i < length) {        //逐个读取事件
        struct inotify_event *event = (struct inotify_event *)&buff[i];
        if (0 != event->len) {      //第i个事件不为空，event->len为结构体成员name[]的长度
            inotify_msg = getmsg(event);     //获取监控到的事件内容并且存储到结构体中
            sever_send(sockfd[1], inotify_msg);        //发送监控到的事件数据结构体
            
            int cmp_type = strncmp(inotify_msg->ftype, "regular", 7);
            int cmp_del = strncmp(inotify_msg->action, "delete", 6);
            if ((0 == cmp_type) && (0 != cmp_del)) {
                send_file_data(*inotify_msg);    //发送文件内容
            }               
        }

        i = i + event->len + EVENT_SIZE;
    }
}

static void dir_inotify(char **dir_storage)
{
    fd = inotify_init();        //初始化
    if (fd < 0) {
        (void)close(fd);
        printf("inotify_init error, err(%d:%s)\n", errno, strerror(errno));
        return;
    }
    
    int wd = 0;
    for (int i = 0; i < dir_index; i++) {       //添加监控路径
        wd = inotify_add_watch(fd, dir_storage[i], IN_DELETE | IN_CREATE | IN_MODIFY | IN_MOVE);
    }

    while (1) {     //循环监控
        event_read();
    }
}

static int callback(const char *fpath, struct stat *statbuff, int typeflag)      //ftw的回调函数
{
    //存储所有文件信息
    memset(&allfile_msg, 0, sizeof(struct file_msg));

    strcpy(allfile_msg.fpath, fpath);       //存储所有文件绝对路径 
    strncpy(allfile_msg.action, "create", 6);

    lstat(fpath, statbuff);     //获取文件信息

    if (S_ISDIR(statbuff->st_mode)) {
        strncpy(allfile_msg.ftype, "dir", 3);       
    }
    else if (S_ISLNK(statbuff->st_mode)) {
        strncpy(allfile_msg.ftype, "link", 4);
        (void)readlink(fpath, allfile_msg.fdata.data, sizeof(allfile_msg.fdata.data));       
    }
    else {
        strncpy(allfile_msg.ftype, "regular", 7);       
    }

    sever_send(sockfd[1], &allfile_msg);
    if (0 == strncmp(allfile_msg.ftype, "regular", 7)) {
        send_file_data(allfile_msg);    //发送文件内容
    }

    //存储文件夹信息
    if (S_ISDIR(statbuff->st_mode)) {
        dir_push(fpath);
    }    

    return 0;
}

int main(int argc, char *argv[])
{   
    sockfd = sever_link("172.31.159.18", 7777);

    dir_storage = (char **)malloc(dir_max * sizeof(char *));        //为指针数组分配指针的指针
    if (NULL == dir_storage) {
        printf("dir_storage malloc error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    int ftwed = ftw("/home/long/dir0", callback, 10);        //遍历目录
    if (0 != ftwed) {
        printf("ftw error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    dir_inotify(dir_storage);

    for (int i = 0; i < dir_max; i++) {     // 释放内存
        free(dir_storage[i]); // 释放 dir_msg 结构中的字符串
    }
    
    free(dir_storage); // 释放 dir_storage 数组

    sever_close(sockfd);

    return 0;
}