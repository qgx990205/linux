#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void mypwd()
{
    char buf[1024];

    // char *getcwd(char *buf,size_t size);
    // getcwd()将当前工作的绝对目录复制到buf所指的内存空间，参数size为buf的空间大小
    
    char *cwd = getcwd(buf, sizeof(buf));

    if (cwd==NULL)
    {
        // 当前路径为NULL,输出错误
        perror("Get cerrent working directory fail.\n");
        exit(-1);
    }
    else
    {
        // 当前路径不为空，输出路径
        printf("%s\n", cwd);
    }
}

int main(int argc,char *argv[])
{
    mypwd();
    return 0;
}