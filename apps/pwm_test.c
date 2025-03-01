/*********************************************************************************
 *      Copyright:  (C) 2025 Liao Shengli<liaoshengli@gmail.com>
 *                  All rights reserved.
 *
 *       Filename:  pwm_test.c
 *    Description:  This file 
 *                 
 *        Version:  1.0.0(01/12/2025)
 *         Author:  Liao Shengli <liaoshengli@gmail.com>
 *      ChangeLog:  1, Release initial version on "01/12/2025 08:30:04 PM"
 *                 
 ********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static char pwm_path[100];

/*pwm 配置函数 attr：属性文件名字
 *  val：属性的值(字符串)
*/
static int pwm_config(const char *attr, const char *val)
{
    char file_path[100];
    int len;
    int fd =  -1;

    if(attr == NULL || val == NULL)
    {
        printf("[%s] argument error\n", __FUNCTION__);
        return -1;
    }

    memset(file_path, 0, sizeof(file_path));
    snprintf(file_path, sizeof(file_path), "%s/%s", pwm_path, attr);
    if (0 > (fd = open(file_path, O_WRONLY))) {
        printf("[%s] open %s error\n", __FUNCTION__, file_path);
        return fd;
    }

    len = strlen(val);
    if (len != write(fd, val, len)) {
        printf("[%s] write %s to %s error\n", __FUNCTION__, val, file_path);
        close(fd);
        return -2;
    }

    close(fd);  //关闭文件
    return 0;
}

int main(int argc, char *argv[])
{
    char temp[100];
    int fd = -1;

    /* 校验传参 */
    if (4 != argc) {
        printf("usage: %s <id> <period> <duty>\n", argv[0]);
        exit(-1);  /* exit(0) 表示进程正常退出 exit(非0)表示异常退出*/
    }

    /* 打印配置信息 */
    printf("PWM config: id<%s>, period<%s>, duty<%s>\n", argv[1], argv[2], argv[3]);

    /* 导出pwm 首先确定最终导出的文件夹路径*/
    memset(pwm_path, 0, sizeof(pwm_path));
    snprintf(pwm_path, sizeof(pwm_path), "/sys/class/pwm/pwmchip%s/pwm0", argv[1]);

    //如果pwm0目录不存在, 则导出
    memset(temp, 0, sizeof(temp));
    if (access(pwm_path, F_OK)) {
        snprintf(temp, sizeof(temp) , "/sys/class/pwm/pwmchip%s/export", argv[1]);
        if (0 > (fd = open(temp, O_WRONLY))) {
            printf("open pwmchip%s error\n", argv[1]);
            exit(-1);
        }
        //导出pwm0文件夹
        if (1 != write(fd, "0", 1)) {
            printf("write '0' to  pwmchip%s/export error\n", argv[1]);
            close(fd);
            exit(-2);
        }

        close(fd);
    }

    /* 配置PWM周期 */
    if (pwm_config("period", argv[2]))
        exit(-1);

    /* 配置占空比 */
    if (pwm_config("duty_cycle", argv[3]))
        exit(-1);

    /* 使能pwm */
    pwm_config("enable", "1");

    return 0;
}
