#include <arpa/inet.h>
#include <errno.h>
#include <ftw.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/types.h>

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

struct file_msg fmsg = {0};
unsigned int dir_max = 4;
unsigned int dir_index = 0;
char **path_storage = NULL;

static void write_file(struct file_msg fmsg)
{  
    if (0 == strncmp(fmsg.fdata.is_data, "yes", 3)) {
        FILE *file =  NULL;

        file = fopen(fmsg.fpath, "a");   //打开文件
        if (NULL == file) {
            printf("fopen error, err(%d:%s)\n", errno, strerror(errno));
            return;
        }
        
        fseek(file, 0, SEEK_END);
        fwrite(fmsg.fdata.data, sizeof(char), sizeof(fmsg.fdata.data), file);
        printf("%s\n", fmsg.fdata.data);
        if (0 != ferror(file)) {
            (void)fclose(file);
            printf("fwrite error, err(%d:%s)\n", errno, strerror(errno));
            return;
        }

        (void)fclose(file);   //关闭文件
    }
}

static int callback(const char *fpath, struct stat *statbuff, int typeflag)      //ftw的回调函数
{    
    //存储夹信息
    if ((dir_index + 1) > (dir_max - 1)) {       //防止开辟空间不够用 
        dir_max = dir_index + 2;
        path_storage = realloc(path_storage, dir_max * sizeof(char *));
        if (NULL == path_storage) {
            printf("path_storage realloc error, err(%d:%s)\n", errno, strerror(errno));
            return -1;
        }            
    }

    path_storage[dir_index++] = strdup(fpath);      //存储目录绝对地址 
    printf("%s\n", fpath);

    return 0;
}

static void removedir(char *path)
{
    int rmed = rmdir("/home/long/dir1");   //删除空文件夹
    if (0 != rmed) {
        path_storage = (char **)malloc(dir_max * sizeof(char *));        //为指针数组分配指针的指针
        if (NULL == path_storage) {
            printf("path_storage malloc error, err(%d:%s)\n", errno, strerror(errno));
            return;
        }  

        int ftwed = ftw("/home/long/dir1", callback, 10);        //遍历目录
        if (0 != ftwed) {
            printf("ftw error, err(%d:%s)\n", errno, strerror(errno));
            return;
        }

        for (int i = dir_index - 1; i >= 0; i--) {
            struct stat statbuf = {0};
            lstat(path_storage[i], &statbuf);

            if (S_ISDIR(statbuf.st_mode)) {
                (void)rmdir(path_storage[i]);   //删除空文件夹
            }
            else {
                (void)remove(path_storage[i]);   //删除文件
            }
        }

        for (int i = 0; i < dir_max; i++) {     // 释放内存
            free(path_storage[i]);
        }
        free(path_storage);             
    }
}

static void sync_create(struct file_msg fmsg)
{
    printf("%s\n", fmsg.fpath);
    if (0 == strncmp(fmsg.ftype, "dir", 3)) {
        (void)mkdir(fmsg.fpath, 0777);
    }
    else if (0 == strncmp(fmsg.ftype, "link", 4)) {
        int symlinked = symlink(fmsg.fdata.data, fmsg.fpath);
        if (symlinked < 0) {
            printf("symlink error, err(%d:%s)\n", errno, strerror(errno));
            return;
        }
    }
    else if (0 == strncmp(fmsg.ftype, "regular", 7)) {
        printf("create regular\n");
        write_file(fmsg);
    }
}

static void sync_delete(struct file_msg fmsg)
{
    if (0 == strncmp(fmsg.ftype, "dir", 3)) {
        (void)rmdir(fmsg.fpath); 
    }
    else {
        (void)remove(fmsg.fpath);   //删除文件
    }   
}

static void sync_modify(struct file_msg fmsg)
{
    if (0 == strncmp(fmsg.ftype, "regular", 7)) {
        (void)remove(fmsg.fpath);   //删除文件
        write_file(fmsg);   //重新创建
    }
}

static void sync_rename(struct file_msg fmsg)
{
    
}

static void sync_all(struct file_msg fmsg)
{
    if (0 == strncmp(fmsg.action, "create", 6)) {
        sync_create(fmsg);
    }
    else if (0 == strncmp(fmsg.action, "delete", 6)) {
        sync_delete(fmsg);
    }
    else if (0 == strncmp(fmsg.action, "modify", 6)) {
        sync_modify(fmsg);
    }
    else if (0 == strncmp(fmsg.action, "rename", 6)) {
        sync_rename(fmsg);
    }
}

static int sever_link(char *ip_addr, char *ip_port)
{
    int sockfd = 0;
    struct sockaddr_in addr = {0};
    socklen_t addrlen = sizeof(addr);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_addr);
    addr.sin_port = htons(atoi(ip_port));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("socket error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    int connected = connect(sockfd, (struct sockadrr *)&addr, addrlen);
    if (connected < 0) {
        (void)close(sockfd);
        printf("connect error, err(%d:%s)\n", errno, strerror(errno));
        return -1;
    }

    return sockfd;
}

static void sever_recv(int sockfd)
{
    while (1) {
        size_t bytes = 0;
        bytes = recv(sockfd, (char *)&fmsg, sizeof(struct file_msg), 0);
        if (bytes  < 0) {
            printf("recv error, err(%d:%s)\n", errno, strerror(errno));
            break;
        }
        else if (bytes > 0) {
            // printf("%s\n", fmsg.fpath);
            sync_all(fmsg);
        }
    }
}

static void sever_close(int sockfd)
{
    (void)close(sockfd);
}

int main(int argc, char *argv[])
{
    int sockfd;

    sockfd = sever_link(argv[1], argv[2]);

    sever_recv(sockfd);

    sever_close(sockfd);

    return 0;
}