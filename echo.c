#include <stdio.h>
int main(int argc, char *argv[])
{
    int i;
    // 遍历输出
    for (i = 1; i < argc; i++)
        printf("%s", argv[i]);
    printf("\n");
    return 0;
}

// 没有2.txt

/* 
按照C语言的约定，argv[0]的值是启动该程序的程序名，
因此argc的值至少为1。如果argc的值为1，则说明程序名后面没有命令行参数。会输出一个换行
在上面的例子中，argc的值为3，argv[0]、argv[1]和argv[2]的值分别为“echo”、“hello,”，以及“world”。
第一个可选参数为argv[1]，而最后一个可选参数为argv[argc-1]。
另外，ANSI标准要求argv[argc]的值必须为一空指针(参见图5-11)。 
*/