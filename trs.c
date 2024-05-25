#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_PATH 1024

char *stack[MAX_PATH] = {0};
unsigned int stack_top = 0;

struct addr {
    char abs_addr_src[MAX_PATH];
    char abs_addr_dst[MAX_PATH];
};

static void stack_push(char *stack[], unsigned int *stack_top, char *dir_path)
{
    stack[(*stack_top)++] = strdup(dir_path);
}

static void stack_pop(char **stack, unsigned int *stack_top, char **pop_path)
{
    *pop_path = stack[--(*stack_top)];
}

static struct addr *abs_addr(struct dirent *entry, char *path_src, char *path_dst, char *pop_path)
{   
    static struct addr addrs = {0};
    (void)snprintf(addrs.abs_addr_src, MAX_PATH, "%s/%s", pop_path, entry->d_name); //输出源绝对地址

    int addr_len = strlen(path_src);
    (void)snprintf(addrs.abs_addr_dst, MAX_PATH, "%s%s", path_dst, &addrs.abs_addr_src[addr_len]);  //输出目的绝对地址

    return &addrs;
}

static bool file_exist(char *path)
{   
    int exist_falg = access(path, F_OK);
    if (0 == exist_falg) {
        return true;
    }
    else if(-1 == exist_falg) {
        return false;
    }
    else {
        perror("access error");
        printf("Error code: %d\n", errno);
    }
} 

static void copy_linkfile(struct addr *addrs)
{
    char buff[1024] = {0};
    (void)readlink(addrs->abs_addr_src, buff, sizeof(buff));    //读链接

    int symlink_flag = symlink(buff, addrs->abs_addr_dst);  //生成链接文件
    if (-1 == symlink_flag) {
        perror("symlink error");
        printf("Error code: %d\n", errno);
    }
}

static void copy_otherfile(struct addr *addrs)
{
    FILE *fp_dst =  NULL;
    FILE *fp_src =  NULL;

    fp_src = fopen(addrs->abs_addr_src, "r");   //打开文件
    if (NULL == fp_src) {
        perror("file_csr fail open\n");
        printf("Error code: %d\n", errno);
        return;
    }

    fp_dst = fopen(addrs->abs_addr_dst, "a+");
    if (NULL == fp_dst) {
        perror("file_dst fail open\n");
        printf("Error code: %d\n", errno);
        fclose(fp_src);
        return;
    }

    char buff[1024] = {0};
    int rd_num = 0;
    
    while(rd_num = fread(buff, sizeof(char), sizeof(buff), fp_src)) {    //读文件
        if (0 != ferror(fp_src)) {
            perror("read error");
        }

        fwrite(buff, sizeof(char), rd_num, fp_dst);     //写文件
        if (0 != ferror(fp_dst)) {
            perror("write error");
        }

        if (rd_num < sizeof(buff)) {
            break;
        }
    }

    (void)fclose(fp_src);   //关闭文件
    (void)fclose(fp_dst);
}

static void copy_allfile(struct dirent *entry, struct addr *addrs)
{
    bool exist_bool = file_exist(addrs->abs_addr_dst);  //判断目标目录中是否有

    if (DT_DIR == entry->d_type) {
        stack_push(stack, &stack_top, addrs->abs_addr_src);    //入栈，存入更目录

        if (!exist_bool) {
            mkdir(addrs->abs_addr_dst, 0755);   //生成文件夹
        }
    }
    else if (DT_LNK == entry->d_type) {
        if (!exist_bool) {
            copy_linkfile(addrs);   //生成链接文件
        }
    }
    else {
        if (!exist_bool) {
            copy_otherfile(addrs);  //生成其他文件
        }
    }
}

static void del_redunant(struct dirent *entry, struct addr *addrs)
{
    bool exist_bool = file_exist(addrs->abs_addr_dst);  //判断目标目录中是否有

    if (DT_DIR == entry->d_type) {
        stack_push(stack, &stack_top, addrs->abs_addr_src);  //入栈，存入更目录

        if (!exist_bool) {
            (void)rmdir(addrs->abs_addr_src);   //删除冗余文件夹
        }
    }
    else {
        if (!exist_bool) {
            (void)remove(addrs->abs_addr_src);      //删除冗余文件
        }
        else {
            struct stat file_type_scr = {0};
            struct stat file_type_des = {0};
            lstat(addrs->abs_addr_src, &file_type_scr);
            lstat(addrs->abs_addr_dst, &file_type_des);
            if (file_type_des.st_mode != file_type_scr.st_mode) {   //如果文件名相同，但文件类型不同，则删除
                (void)remove(addrs->abs_addr_src);
            }
        }
    }
}

static void traversal(DIR *opdir, char *path_src, char *path_dst, char *pop_path, char *mode)
{
    for (struct dirent *entry = readdir(opdir); NULL != entry; entry = readdir(opdir)) {
        int cmp_cur = strcmp(entry->d_name, ".");
        int cmp_parent = strcmp(entry->d_name, "..");
        if ((0 == cmp_cur) || (0 == cmp_parent)) {      //跳过当前、上一级目录
            continue;
        }
        
        struct addr *addrs = abs_addr(entry, path_src, path_dst, pop_path); //生成绝对地址
        
        if (0 == strcmp(mode, "copy")) {    //同步文件
            copy_allfile(entry, addrs);
        }
        else if (0 == strcmp(mode, "del")) {    //删除冗余
            del_redunant(entry, addrs);
        }
    }
}

static void dfstraversal(char *path_src, char *path_dst, char *mode)
{
    bool exist_bool = file_exist(path_dst);
    if(!exist_bool) {        //如果目的根目录不存在，则新建
        mkdir(path_dst, 0755);
    }

    stack_push(stack, &stack_top, path_src);    //入栈，存入更目录

    while (stack_top > 0) {
        char *pop_path = NULL;
        stack_pop(stack, &stack_top, &pop_path);    // 弹栈

        DIR *opdir = opendir(pop_path);     // 打开文件夹
        if (NULL == opdir) {
            perror("opendir error");
        }

        traversal(opdir, path_src, path_dst, pop_path, mode);   // 遍历源文件夹，并同步文件

        (void)closedir(opdir);      // 关闭文件夹
        
        (void)free(stack[(stack_top)]);       //释放栈内存
        stack[(stack_top)] = NULL;
    }
}

int main(int argc, char *argv[])
{
    if(3 != argc) {
        printf("%s","The number of entered addresses is incorrect");
        return 0;
    }

    while(1) {    
        bool exist_bool = file_exist(argv[2]);
        if(exist_bool) {     //如果目的根目录存在，则删除冗余
            dfstraversal(argv[2], argv[1], "del");//删除冗余
        }

        dfstraversal(argv[1], argv[2], "copy");//同步文件
        sleep(1);
    }

    // while(1) {    
    //     bool exist_bool = file_exist("/home/long/dir1");
    //     if(exist_bool) {     //如果目的根目录存在，则删除冗余
    //         dfstraversal("/home/long/dir1", "/home/long/dir0", "del");//删除冗余
    //     }

    //     dfstraversal("/home/long/dir0", "/home/long/dir1", "copy");//同步文件
    //     sleep(1);
    // }

    return 0;
}