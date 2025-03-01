/*********************************************************************************
 *      Copyright:  (C) 2025 Liao Shengli<liaoshengli@gmail.com>
 *                  All rights reserved.
 *
 *       Filename:  keypad1.c
 *    Description:  This file 
 *                 
 *        Version:  1.0.0(01/12/2025)
 *         Author:  Liao Shengli <liaoshengli@gmail.com>
 *      ChangeLog:  1, Release initial version on "01/12/2025 03:36:07 PM"
 *                 
 ********************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/keyboard.h>

#if 0 /* Just for comment here, Reference to linux-3.3/include/linux/input.h */
struct input_event
{
    struct timeval time;
    __u16 type;  /* 0x00:EV_SYN 0x01:EV_KEY 0x04:EV_MSC 0x11:EV_LED*/
    __u16 code;  /* key value, which key */
    __s32 value; /* 1: Pressed  0:Not pressed  2:Always Pressed */
};
#endif

#define EV_RELEASED        0
#define EV_PRESSED         1

#define BUTTON_CNT         10

/* 在C语言编程中，函数应该先定义再使用，如果函数的定义在函数调用后面，应该前向声明。*/
void usage(char *name);

void display_button_event(struct input_event *ev, int cnt);

int main(int argc, char **argv)
{
    char                  *kbd_dev = "/dev/input/event1";  //默认监听按键设备；
    char                  kbd_name[256] = "Unknown"; //用于保存获取到的设备名称
    int                   kbd_fd = -1;  //open()打开文件的文件描述符
    int                   rv=0;  // 函数返回值，默认返回0；
    int                   opt;    // getopt_long 解析命令行参数返回值；
    int                   size = sizeof (struct input_event);
    fd_set                rds; //用于监听的事件的集合

    struct input_event    ev[BUTTON_CNT];

    /* getopt_long参数函数第四个参数的定义，二维数组，每个成员由四个元素组成 */
    struct option long_options[] = {
         /* { 参数名称，是否带参数，flags指针(NULL时将val的数值从getopt_long的返回值返回出去),
            函数找到该选项时的返回值(字符)}
         */
        {"device", required_argument, NULL, 'd'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    //获取命令行参数的解析返回值
    while ((opt = getopt_long(argc, argv, "d:h", long_options, NULL)) != -1)
    {
        switch (opt)
        {
            case 'd':
                kbd_dev = optarg;
                break;

            case 'h':
                usage(argv[0]);
                return 0;

            default:
                break;
        }
    }

    if(NULL == kbd_dev)
    {
        /* 命令行argv[0]是输入的命令，如 ./keypad */
        usage(argv[0]);
        return -1;
    }

    /* 获取uid 建议以root权限运行确保可以正常运行 */
    if ((getuid ()) != 0)
        printf ("You are not root! This may not work...\n");

    /* 打开按键对应的设备节点，如果错误则返回负数 */
    if ((kbd_fd = open(kbd_dev, O_RDONLY)) < 0)
    {
        printf("Open %s failure: %s", kbd_dev, strerror(errno));
        return -1;
    }

    /* 使用ioctl获取 /dev/input/event*对应的设备名字 */
    ioctl (kbd_fd, EVIOCGNAME (sizeof (kbd_name)), kbd_name);
    printf ("Monitor input device %s (%s) event on poll mode:\n", kbd_dev, kbd_name);

    /* 循环使用 select() 多路复用监听按键事件 */
    while (1)
    {
        FD_ZERO(&rds); /* 清空 select() 的读事件集合 */
        FD_SET(kbd_fd, &rds); /* 将按键设备的文件描述符加入到读事件集合中*/

        /* 使用select开启监听并等待多个描述符发生变化，第一个参数最大描述符+1，
           2、3、4参数分别是要监听读、写、异常三个事件的文军描述符集合；
           最后一个参数是超时时间(NULL-->永不超时，会一直阻塞住)

           如果按键没有按下，则程序一直阻塞在这里。一旦按键按下，则按键设备有数据
           可读，此时函数将返回。
        */
        rv = select(kbd_fd + 1, &rds, NULL, NULL, NULL);
        if (rv < 0)
        {
            printf("Select() system call failure: %s\n", strerror(errno));
            goto CleanUp;
        }
        else if (FD_ISSET(kbd_fd, &rds)) /* 是按键设备发生了事件 */
        {
            //read读取input设备的数据包，数据包为input_event结构体类型。
            if ((rv = read (kbd_fd, ev, size*BUTTON_CNT )) < size)
            {
                printf("Reading data from kbd_fd failure: %s\n", strerror(errno));
                break;
            }
            else
            {
                display_button_event(ev, rv/size);
            }
        }
    }

CleanUp:
    close(kbd_fd);

    return 0;
}

/* 该函数用来打印程序的使用方法 */
void usage(char *name)
{
    char *progname = NULL;
    char *ptr = NULL;

    /* 字符串拷贝函数，该函数内部将调用malloc()来动态分配内存，然后将$name
       字符串内容拷贝到malloc分配的内存中，这样使用完之后需要free释放内存. */
    ptr = strdup(name);
    progname = basename(ptr); //去除该可执行文件的路径名，获取其自身名称(即keypad)

    printf("Usage: %s [-p] -d <device>\n", progname);
    printf(" -d[device  ] button device name\n");
    printf(" -p[poll    ] Use poll mode, or default use infinit loop.\n");
    printf(" -h[help    ] Display this help information\n");

    free(ptr);  //和strdup对应，释放该内存
    return;
}

/* 该函数用来解析按键设备上报的数据，并答应按键按下的相关信息 */
void display_button_event(struct input_event *ev, int cnt)
{
    int i;
    static struct timeval pressed_time;  //该变量用来存放按键按下的时间，注意static的使用。
    struct timeval        duration_time; //该变量用来存放按键按下持续时间

    for(i=0; i<cnt; i++)
    {
        /* 当上报的时间type为EV_KEY时候并且，value值为1或0 (1为按下，0为释放) */
        if(EV_KEY==ev[i].type && EV_PRESSED==ev[i].value)
        {
            pressed_time = ev[i].time;
            printf("Keypad[%d] pressed time: %ld.%ld\n",
                   ev[i].code, pressed_time.tv_sec, pressed_time.tv_usec);
        }
        if(EV_KEY==ev[i].type && EV_RELEASED==ev[i].value)
        {
            /* 计算时间差函数 将第一个参数减去第二个参数的值的结果 放到第三个参数之中 */
            timersub(&ev[i].time, &pressed_time, &duration_time);
            printf("keypad[%d] released time: %ld.%ld\n",
                   ev[i].code, ev[i].time.tv_sec, ev[i].time.tv_usec);
            printf("keypad[%d] duration time: %ld.%ld\n",
                   ev[i].code, duration_time.tv_sec, duration_time.tv_usec);
        }
    }
}
