/*
实现了以下ls的参数

ls -a可以将目录下的全部文件（包括隐藏文件）显示出来
ls -l 列出长数据串，包括文件的属性和权限等数据
ls -r将排序结果反向输出，例如：原本文件名由小到大，反向则由大到小
ls -R连同子目录一同显示出来，也就所说该目录下所有文件都会显示出来（显示隐藏文件要加-a参数） 
*/

/*
关于stat
struct stat的结构:
struct stat {
    dev_t     st_dev;      文件的设备编号
    ino_t     st_ino;      节点
    mode_t    st_mode;     文件的类型和权限
    nlink_t   st_nlink;    连到该文件的硬链接数目，刚建立的文件值为1
    uid_t     st_uid;      用户ID
    gid_t     st_gid;      组ID
    dev_t     st_rdev;     设备类型）若此文件为设备文件，则为其设备编号
    off_t     st_size;     文件字节数（文件大小）
    blksize_t st_blksize;  块大小（文件I/O缓冲区的大小）
    blkcnt_t  st_blocks;   块数
    time_t    st_atime;    最后一次访问时间
    time_t    st_mtime;    最后一次修改时间
    time_t    st_ctime;    最后一次改变时间（指属性）
};

    

其中，st_mode的类型 mode_t.
mode_t其实就是普通的unsigned int.
st_mode使用了其低19bit. 0170000 => 1+ 3*5 = 16.
其中，最低的9位(0-8)是权限，9-11是id，12-15是类型。
当我们需要快速获得文件类型或访问权限时，最好的方法就是使用glibc定义的宏。
如：S_ISDIR，S_IRWXU等。

一些具体的宏
S_IFMT 0170000 文件类型的位遮罩
S_IFSOCK    0140000    socket
S_IFLNK    0120000    符号链接(symbolic link)
S_IFREG    0100000    一般文件
S_IFBLK    0060000    区块装置(block device)
S_IFDIR    0040000    目录
S_IFCHR    0020000    字符装置(character device)
S_IFIFO    0010000    先进先出(fifo)
S_ISUID    0004000    文件的(set user-id on execution)位
S_ISGID    0002000    文件的(set group-id on execution)位
S_ISVTX    0001000    文件的sticky位
S_IRWXU    00700      文件所有者的遮罩值(即所有权限值)
S_IRUSR    00400      文件所有者具可读取权限
S_IWUSR    00200      文件所有者具可写入权限
S_IXUSR    00100      文件所有者具可执行权限
S_IRWXG    00070      用户组的遮罩值(即所有权限值)
S_IRGRP    00040      用户组具可读取权限
S_IWGRP    00020      用户组具可写入权限
S_IXGRP    00010      用户组具可执行权限
S_IRWXO    00007      其他用户的遮罩值(即所有权限值)
S_IROTH    00004      其他用户具可读取权限
S_IWOTH    00002      其他用户具可写入权限
S_IXOTH    00001      其他用户具可执行权限

    // S_ISLNK(st_mode)：是否是一个连接.
    // S_ISREG(st_mode)：是否是一个常规文件.
    // S_ISDIR(st_mode)：是否是一个目录
    // S_ISCHR(st_mode)：是否是一个字符设备.
    // S_ISBLK(st_mode)：是否是一个块设备
    // S_ISFIFO(st_mode)：是否 是一个FIFO文件.
    // S_ISSOCK(st_mode)：是否是一个SOCKET文件

*/

/*
passwd 结构体定义在<pwd.h>中，结构如下：
struct passwd
{
  char *pw_name;                /* 用户登录名 * /
  char *pw_passwd;              /* 密码(加密后) * /
  __uid_t pw_uid;               /* 用户ID * /
  __gid_t pw_gid;               /* 组ID * /
  char *pw_gecos;               /* 详细用户名 * /
  char *pw_dir;                 /* 用户目录 * /
  char *pw_shell;               /* Shell程序名 * /
};

在#include <grp.h>
struct group
{
  char *gr_name;  				/* 组名 * /
  char *gr_passwd;  			/* 密码 * /
  __gid_t gr_gid;  				/* 组ID * /
  char **gr_mem;  				/* 组成员名单 * /
}

struct dirent
{
long d_ino; /* 索引节点号 * /
off_t d_off; /* 在目录文件中的偏移 * /
unsigned short d_reclen; /* 文件名长 * /
unsigned char d_type; /* 文件类型 * /
char d_name [NAME_MAX+1]; /* 文件名，最长256字符 * /
}

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>

// 定义固定值（用于后面的参数确定）
#define PARAM_NONE 0 //由于后面要用到位运算，所以介绍一下，当没有参数的时候flag=0；然后依次是-a,-l,-R,-r，定义为1,2,4,8
#define PARAM_A 1    //刚好就是二进制中的1，10，100，1000。这样方便为运算中的|和&，比如同时有a和r参数，那么flag就为1001.
#define PARAM_L 2    //当要判断是否含有这两个参数中的一个时(比如r参数)就可以用flag&PARAM_r,如果为0的话就没有r这个参数。
#define PARAM_R 4    //其他的也类似
#define PARAM_r 8
#define MAXROWLEN 80

char PATH[PATH_MAX + 1]; //用于存储路径
int flag;                // 用于判断参数

int g_leave_len = MAXROWLEN;
int g_maxlen;

void my_err(const char *err_string, int line); // 错误
void display_dir(char *path);                  // 展示文件夹

void my_err(const char *err_string, int line)
{
    // err_string:具体的错误

    fprintf(stderr, "line:%d", __LINE__);
    /*
    stdett:标准输出(设备)文件，对应终端的屏幕。进程将从标准输入文件中得到输入数据，将正常输出数据输出到标准输出文件，而将错误信息送到标准错误文件中。在C中，程序执行时，一直处于开启状态。
    */
    perror(err_string);
    // perror(s)用来将上一个函数发生错误的原因输出到标准设备(stderr)。
    // 参数 s 所指的字符串会先打印出，后面再加上错误原因字符串
    exit(1);
}

void cprint(char *name, mode_t st_mode) // 用于控制输出文字的颜色
{
    // 判断类型，然后根据类型确定颜色
    // st_mode 用于判断权限，具体用法在上方注释
    // S_ISLNK(st_mode)：是否是一个连接.
    // S_ISDIR(st_mode)：是否是一个目录
    if (S_ISLNK(st_mode))                                    //链接文件
        printf("\033[1;36m%-*s  \033[0m", g_maxlen, name);     // 各种颜色控制
    else if (S_ISDIR(st_mode) && (st_mode & 000777) == 0777) //满权限的目录
        printf("\033[1;34;42m%-*s  \033[0m", g_maxlen, name);
    else if (S_ISDIR(st_mode)) //目录
        printf("\033[1;34m%-*s  \033[0m", g_maxlen, name);
    else if (st_mode & S_IXUSR || st_mode & S_IXGRP || st_mode & S_IXOTH) //可执行文件
        printf("\033[1;32m%-*s  \033[0m", g_maxlen, name);
    else //其他文件，直接输出，默认颜色
        printf("%-*s ", g_maxlen, name);
}

void display_attribute(char *name) //-l参数时按照相应格式打印
{
    struct stat buf; // buf 为stat类型的结构体，结构体的具体属性看上方注释
    char buff_time[32];
    struct passwd *psd; //从该结构体接收文件所有者用户名
    struct group *grp;  //获取组名

    if (lstat(name, &buf) == -1) // lstat() 函数返回关于文件或符号连接的信息。
    {
        // 如果信息不正确就是错误。
        my_err("stat", __LINE__);
    }

    // S_ISLNK(st_mode)：是否是一个连接.
    // S_ISREG(st_mode)：是否是一个常规文件.
    // S_ISDIR(st_mode)：是否是一个目录
    // S_ISCHR(st_mode)：是否是一个字符设备.
    // S_ISBLK(st_mode)：是否是一个块设备
    // S_ISFIFO(st_mode)：是否 是一个FIFO文件.
    // S_ISSOCK(st_mode)：是否是一个SOCKET文件

    // 确认一些列具体情况并打印
    // 打印文件的类型
    if (S_ISLNK(buf.st_mode))
        printf("l");
    else if (S_ISREG(buf.st_mode))
        printf("-");
    else if (S_ISDIR(buf.st_mode))
        printf("d");
    else if (S_ISCHR(buf.st_mode))
        printf("c");
    else if (S_ISBLK(buf.st_mode))
        printf("b");
    else if (S_ISFIFO(buf.st_mode))
        printf("f");
    else if (S_ISSOCK(buf.st_mode))
        printf("s");

    //获取打印文件所有者权限
    if (buf.st_mode & S_IRUSR)
        printf("r");
    else
        printf("-");
    if (buf.st_mode & S_IWUSR)
        printf("w");
    else
        printf("-");
    if (buf.st_mode & S_IXUSR)
        printf("x");
    else
        printf("-");

    //所有组权限
    if (buf.st_mode & S_IRGRP)
        printf("r");
    else
        printf("-");
    if (buf.st_mode & S_IWGRP)
        printf("w");
    else
        printf("-");
    if (buf.st_mode & S_IXGRP)
        printf("x");
    else
        printf("-");

    //其他人权限
    if (buf.st_mode & S_IROTH)
        printf("r");
    else
        printf("-");
    if (buf.st_mode & S_IWOTH)
        printf("w");
    else
        printf("-");
    if (buf.st_mode & S_IXOTH)
        printf("x");
    else
        printf("-");

    printf(" ");

    //根据uid和gid获取文件所有者的用户名和组名
    // getpwuid 根据传入的用户ID返回指向passwd的结构体 该结构体初始化了里面的所有成员
    // getgrgid()用来依参数gid 指定的组识别码逐一搜索组文件, 找到时便将该组的数据以group 结构返回. 返回值：返回 group 结构数据, 如果返回NULL 则表示已无数据, 或有错误发生.
    psd = getpwuid(buf.st_uid);    // 获取所有者用户名
    grp = getgrgid(buf.st_gid);    // 获取组名
    printf("%4ld ", buf.st_nlink); //链接数
    printf("%-8s ", psd->pw_name); // 打印用户登录名
    printf("%-8s ", grp->gr_name); // 打印组名
    printf("%6ld ", buf.st_size);   // 打印文件字节数

    strcpy(buff_time, ctime(&buf.st_mtime)); // ctime功能是 把日期和时间转换为字符串
    // 并且将这个文件时间字符串赋值给buff_time

    buff_time[strlen(buff_time) - 1] = '\0'; // buff_time自带换行，因此要去掉后面的换行符
    printf(" %s ", buff_time);             // 打印文件时间
    cprint(name, buf.st_mode);               // 根据文件的不同类型显示不同颜色
    printf("\n");                            // 换行
}

void displayR_attribute(char *name) //当l和R都有时，先调用display_attribute打印，然后该函数负责递归
// 一般情况下不会出现
{
    struct stat buf;

    if (lstat(name, &buf) == -1)
    {
        my_err("stat", __LINE__);
    }
    if (S_ISDIR(buf.st_mode))
    {
        display_dir(name);
        free(name); // 将之前filenames[i]中的空间释放
        char *p = PATH;
        while (*++p);
        while (*--p != '/');
        *p = '\0';   //每次递归完成之后将原来的路径回退至递归前
        chdir(".."); //跳转到当前目录的上一层目录
        return;
    }
}
void display_single(char *name) //打印文件
{
    int len;
    struct stat buf;
    if (lstat(name, &buf) == -1) // lstat() 函数返回关于文件或符号连接的信息。
    {
        return;
    }

    if (g_leave_len < g_maxlen)
    {
        printf("\n");
        g_leave_len = MAXROWLEN;
    }

    cprint(name, buf.st_mode); // 根据文件的不同类型显示不同颜色
    g_leave_len = g_leave_len - (g_maxlen + 2);
}

void displayR_single(char *name) //当有-R参数时打印文件名并调用display_dir
{
    int len;
    struct stat buf;
    if (lstat(name, &buf) == -1)
    {
        return;
    }
    if (S_ISDIR(buf.st_mode))
    {
        printf("\n");

        g_leave_len = MAXROWLEN;

        display_dir(name);
        free(name); //将之前filenames[i]中的空间释放
        char *p = PATH;
        while (*++p)
            ;
        while (*--p != '/')
            ;
        *p = '\0';
        chdir(".."); //返回上层目录
    }
}

void display(char **name, int count) //根据flag去调用不同的函数
{
    switch (flag)
    {
        int i;
    case PARAM_r:    // 参数为-r
    case PARAM_NONE: // 没有参数，直接打印
        for (i = 0; i < count; i++)
        {
            if (name[i][0] != '.') //排除.., .,以及隐藏文件
                display_single(name[i]);
        }
        break;
    case PARAM_r + PARAM_A: // 参数为-r,-a
    case PARAM_A:           // 参数为-a
        for (i = 0; i < count; i++)
        {
            display_single(name[i]);
        }
        break;
    case PARAM_r + PARAM_L: // 参数为-r,-l
    case PARAM_L:           // 参数为-l
        for (i = 0; i < count; i++)
        {
            if (name[i][0] != '.')
            {
                display_attribute(name[i]);
            }
        }
        break;
    case PARAM_R + PARAM_r: // 参数为-R,-r
    case PARAM_R:           // 参数为-R
        // 先调用-r的函数
        for (i = 0; i < count; i++)
        {
            if (name[i][0] != '.')
            {
                display_single(name[i]);
            }
        }
        // 再调用自身的打印函数
        for (i = 0; i < count; i++)
        {
            if (name[i][0] != '.') //排除 "."和".."两个目录，防止死循环，下同
            {
                displayR_single(name[i]);
            }
        }
        break;
    case PARAM_L + PARAM_r + PARAM_R:
    case PARAM_R + PARAM_L: // 参数为-R，-l
        for (i = 0; i < count; i++)
        {
            if (name[i][0] != '.')
            {
                display_attribute(name[i]);
            }
        }
        for (i = 0; i < count; i++)
        {
            if (name[i][0] != '.')
            {
                displayR_attribute(name[i]);
            }
        }
        break;
    case PARAM_A + PARAM_r + PARAM_R:
    case PARAM_R + PARAM_A:
        for (i = 0; i < count; i++)
        {
            display_single(name[i]);
        }
        for (i = 0; i < count; i++)
        {
            if (name[i][0] != '.')
            {
                displayR_single(name[i]);
            }
        }
        break;

    case PARAM_A + PARAM_L + PARAM_r:
    case PARAM_A + PARAM_L:
        for (i = 0; i < count; i++)
        {
            display_attribute(name[i]);
        }
        break;
    case PARAM_A + PARAM_L + PARAM_R + PARAM_r:
    case PARAM_A + PARAM_L + PARAM_R:
        for (i = 0; i < count; i++)
        {
            display_attribute(name[i]);
        }
        for (i = 0; i < count; i++)
        {
            if (name[i][0] != '.')
            {
                displayR_attribute(name[i]);
            }
        }
        break;

    default:
        break;
    }
}

void display_dir(char *path) // 该函数用以非文件所在目录处理
{
    DIR *dir;           //接受opendir返回的文件描述符
    struct dirent *ptr; //接受readdir返回的结构体
    int count = 0;      // 文件数目
    //char filenames[300][PATH_MAX+1],temp[PATH_MAX+1];  这里是被优化掉的代码，由于函数中定义的变量
    //是在栈上分配空间，因此当多次调用的时候会十分消耗栈上的空间，最终导致栈溢出，在linux上的表现就是核心已转储
    //并且有的目录中的文件数远远大于300

    if ((flag & PARAM_R) != 0) // 判断是否有-R参数
    {
        int len = strlen(PATH); // 存储路径的长度
        if (len > 0)
        {
            if (PATH[len - 1] == '/')
                PATH[len - 1] = '\0'; // 路径为根目录直接结束
        }
        if (path[0] == '.' || path[0] == '/')
        {
            strcat(PATH, path); // 将两个char类型连接
        }
        else
        {
            strcat(PATH, "/");
            strcat(PATH, path);
        }

        printf("%s:\n", PATH); // 打印路径
    }

    //获取文件数和最长文件名长度
    dir = opendir(path);
    if (dir == NULL)
        my_err("opendir", __LINE__);
    g_maxlen = 0;

    while ((ptr = readdir(dir)) != NULL)
    {
        if (g_maxlen < strlen(ptr->d_name))
            g_maxlen = strlen(ptr->d_name); // 最大文件名的长度
        count++;                            // 有多少文件
    }

    closedir(dir);

    char **filenames = (char **)malloc(sizeof(char *) * count), temp[PATH_MAX + 1]; //通过目录中文件数来动态的在堆上分配存储空间，首先定义了一个指针数组
    for (int i = 0; i < count; i++)                                                 //然后依次让数组中每个指针指向分配好的空间，这里是对上面的优化，有效
    {                                                                               //的防止了栈溢出，同时动态分配内存，更加节省空间
        filenames[i] = (char *)malloc(sizeof(char) * PATH_MAX + 1);
    }

    int i, j;
    //获取该目录下所有的文件名
    dir = opendir(path);
    for (i = 0; i < count; i++)
    {
        ptr = readdir(dir);
        if (ptr == NULL)
        {
            my_err("readdir", __LINE__);
        }
        strcpy(filenames[i], ptr->d_name); // 将文件名赋值给filenames数组
    }
    closedir(dir);

    //冒泡法对文件名排序
    if (flag & PARAM_r) //-r参数反向排序
    {
        for (i = 0; i < count - 1; i++)
        {
            for (j = 0; j < count - 1 - i; j++)
            {
                if (strcmp(filenames[j], filenames[j + 1]) < 0)
                {
                    strcpy(temp, filenames[j]);
                    strcpy(filenames[j], filenames[j + 1]);
                    strcpy(filenames[j + 1], temp);
                }
            }
        }
    }
    else //正向排序
    {
        for (i = 0; i < count - 1; i++)
        {
            for (j = 0; j < count - 1 - i; j++)
            {
                if (strcmp(filenames[j], filenames[j + 1]) > 0)
                {
                    strcpy(temp, filenames[j]);
                    strcpy(filenames[j], filenames[j + 1]);
                    strcpy(filenames[j + 1], temp);
                }
            }
        }
    }

    if (chdir(path) < 0)
    {
        my_err("chdir", __LINE__);
    }

    display(filenames, count);

}

int main(int argc, char **argv)
{
    int i, j, k = 0, num = 0; //num值用于统计参数数目的统计

    char param[32] = "";                                     //用来保存命令行参数
    char *path[1];                                           //保存路径，定义为一个指针数组，为了和后面的char **类型的函数参数对应只能定义成这样
    path[0] = (char *)malloc(sizeof(char) * (PATH_MAX + 1)); //由于是一个指针类型，所以我们要给他分配空间  PATH_MAX是系统定义好的宏，它的值为4096,严谨一点，加1用来存'\0'
    // '\0'表示字符结束符，用于结束字符
    flag = PARAM_NONE; //初始化flag=0（由于flag是全剧变量，所以可以不用初始化）
    struct stat buf;   //该结构体以及lstat,stat，在 sys/types.h，sys/stat.h，unistd.h 中，具体 man 2 stat

    //命令行参数解析,将-后面的参数保存至param中
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            for (j = 1; j < strlen(argv[i]); j++)
            {
                param[k] = argv[i][j];
                k++;
            }
            num++;
        }
    }
    param[k] = '\0';

    /* 判断参数 */
    for (i = 0; i < k; i++)
    {
        if (param[i] == 'a')
            flag |= PARAM_A;
        // 这里的意思是flag=flag|PARAM_A,以下类似
        else if (param[i] == 'l')
            flag |= PARAM_L;
        else if (param[i] == 'R')
            flag |= PARAM_R;
        else if (param[i] == 'r')
            flag |= PARAM_r;
        else
        {
            // 这里是参数错误的情况
            printf("my_ls:invalid option -%c\n", param[i]);
            exit(0); // 用于退出
        }
    }

    //如果没有输入目标文件或目录，就显示当前目录
    if (num + 1 == argc)
    {
        strcpy(path[0], "."); // C中的copy，将'.'赋值到path[0]中
        display_dir(path[0]); // display_dir()函数在第318行
		printf("\n");
        return 0;
    }

    i = 1;
    do
    {
        if (argv[i][0] == '-')
        {
            i++;
            continue;
        }
        else
        {
            strcpy(path[0], argv[i]);
            //判断目录或文件是否存在
            if (stat(argv[i], &buf) == -1)
            {
                my_err("stat", __LINE__); //编译器内置的宏，插入当前源码行号
            }
            if (S_ISDIR(buf.st_mode)) //判断是否为目录
            {
                display_dir(path[0]);
                i++;
            }
            else
            {
                // 不是目录，开始打印文件
                display(path, 1);
                i++;
            }
        }
    } while (i < argc);
    printf("\n");

    return 0;
}
