/*********************************************************************************
 *      Copyright:  (C) 2025 Liao Shengli<liaoshengli@gmail.com>
 *                  All rights reserved.
 *
 *       Filename:  pwm_play.c
 *    Description:  This file 
 *                 
 *        Version:  1.0.0(01/12/2025)
 *         Author:  Liao Shengli <liaoshengli@gmail.com>
 *      ChangeLog:  1, Release initial version on "01/12/2025 08:30:52 PM"
 *                 
 ********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#define PWM_CHIP                        1 /* buzzer on pwmchip1 */

typedef struct pwm_note_s
{
	unsigned int    msec; //持续时间，毫秒
	unsigned int    freq;//频率
	unsigned char   duty;//相对占空比，百分比 * 100
}pwm_note_t;

/* 使用宏确定使用中音还是高、低音 */
#define CX                      CM

/* 低、中、高音频率*/
static const unsigned short CL[8] = {0, 262, 294, 330, 349, 392, 440, 494};
static const unsigned short CM[8] = {0, 523, 587, 659, 698, 784, 880, 988};
static const unsigned short CH[8] = {0, 1046, 1175, 1318, 1397, 1568, 1760, 1976};

/* 小星星曲子*/
static unsigned short melody[] = {
	CX[1], CX[1], CX[5], CX[5], CX[6], CX[6], CX[5], CX[0],
	CX[4], CX[4], CX[3], CX[3], CX[2], CX[2], CX[1], CX[0],
	CX[5], CX[5], CX[4], CX[4], CX[3], CX[3], CX[2], CX[0],
	CX[5], CX[5], CX[4], CX[4], CX[3], CX[3], CX[2], CX[0],
};

static char pwm_path[64]; /* pwm file path buffer */

static int pwm_export(int chip);
static int pwm_config(const char *attr, const char *val);
static int pwm_ring(pwm_note_t *note);
static inline void msleep(unsigned long ms);

int main(int argc, char *argv[])
{
	pwm_note_t    note;
	int           i;

	if( pwm_export(PWM_CHIP) < 0 )
		return 1;

	printf("pwm_export() successfully");

	pwm_config("enable", "1");

	for(i=0; i<sizeof(melody)/sizeof(melody[0]); i++)
	{
		if(melody[i] == 0)
		{
			note.duty = 0;
		}
		else
		{
			note.duty = 15; //越大音量越大
			note.freq = melody[i];
		}
		note.msec = 300;

		pwm_ring(&note);
	}

	pwm_config("enable", "0");

	return 0;
}

static int pwm_export(int chip)
{
	char file_path[100];
	int  fd, rv=0;

	/* 导出pwm 首先确定最终导出的文件夹路径*/
	memset(pwm_path, 0, sizeof(pwm_path));
	snprintf(pwm_path, sizeof(pwm_path), "/sys/class/pwm/pwmchip%d/pwm0", PWM_CHIP);

	//如果pwm0 目录已经存在了, 则直接返回
	if ( !access(pwm_path, F_OK))
		return 0;

	//如果pwm0 目录不存在, 则开始导出

	memset(file_path, 0, sizeof(file_path));
	snprintf(file_path, sizeof(file_path) , "/sys/class/pwm/pwmchip%d/export", PWM_CHIP);

	if ( (fd = open(file_path, O_WRONLY) < 0))
	{
		printf("ERROR: open pwmchip%d error\n", PWM_CHIP);
		return 1;
	}

	if ( write(fd, "0", 1) < 0 )
	{
		printf("write '0' to  pwmchip%d/export error\n", PWM_CHIP);
		rv = 2;
	}

	close(fd);
	return rv;
}

/*pwm 配置函数 attr：属性文件名字
 *  val：属性的值(字符串)
 */
static int pwm_config(const char *attr, const char *val)
{
	char file_path[100];
	int fd;
	int len;

	if( !attr || !val )
	{
		printf("[%s] argument error\n", __FUNCTION__);
		return -1;
	}

	memset(file_path, 0, sizeof(file_path));
	snprintf(file_path, sizeof(file_path), "%s/%s", pwm_path, attr);
	if( (fd=open(file_path, O_WRONLY)) < 0 )
	{
		printf("[%s] open %s error\n", __FUNCTION__, file_path);
		return fd;
	}

	len = strlen(val);
	if ( write(fd, val, len) != len) 
	{
		printf("[%s] write %s to %s error\n", __FUNCTION__, val, file_path);
		close(fd);
		return -2;
	}

	close(fd);  //关闭文件
	return 0;
}

/* pwm蜂鸣器响一次声音 */
static int pwm_ring(pwm_note_t *note)
{
	unsigned long period = 0;
	unsigned long duty_cycle = 0;
	char period_str[20] = {};
	char duty_cycle_str[20] = {};

	if( !note || note->duty > 100 )
	{
		printf("[INFO] %s argument error.\n", __FUNCTION__);
		return -1;
	}

	period = (unsigned long)((1.f / (double)note->freq) * 1e9);//ns单位
	duty_cycle = (unsigned long)(((double)note->duty / 100.f) * (double)period);//ns单位

	snprintf(period_str, sizeof(period_str), "%lu", period);
	snprintf(duty_cycle_str, sizeof(duty_cycle_str), "%lu", duty_cycle);

	//设置pwm频率和周期
	if (pwm_config("period", period_str))
	{
		printf("pwm_config period failure.\n");
		return -1;
	}
	if (pwm_config("duty_cycle", duty_cycle_str))
	{
		printf("pwm_config duty_cycle failure.\n");
		return -2;
	}

	msleep(note->msec);

	/* 设置占空比为0 蜂鸣器无声 */
	if (pwm_config("duty_cycle", "0"))
	{
		printf("pwm_config duty_cycle failure.\n");
		return -3;
	}
	msleep(20);

	return 0;
}

/* ms级休眠函数 */
static inline void msleep(unsigned long ms)
{
	struct timespec cSleep;
	unsigned long ulTmp;

	cSleep.tv_sec = ms / 1000;
	if (cSleep.tv_sec == 0)
	{
		ulTmp = ms * 10000;
		cSleep.tv_nsec = ulTmp * 100;
	}
	else
	{
		cSleep.tv_nsec = 0;
	}

	nanosleep(&cSleep, 0);
}
