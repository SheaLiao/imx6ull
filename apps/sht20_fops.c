/*********************************************************************************
 *      Copyright:  (C) 2025 Liao Shengli<liaoshengli@gmail.com>
 *                  All rights reserved.
 *
 *       Filename:  sht20_fops.c
 *    Description:  This file 
 *                 
 *        Version:  1.0.0(01/17/2025)
 *         Author:  Liao Shengli <liaoshengli@gmail.com>
 *      ChangeLog:  1, Release initial version on "01/17/2025 02:45:33 PM"
 *                 
 ********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

int sht20_checksum(uint8_t *data, int len, int8_t checksum);
static inline void msleep(unsigned long ms);
static inline void dump_buf(const char *prompt, uint8_t *buf, int size);

int main(int argc, char **argv)
{
    int             fd, rv;
    float           temp, rh;
    char           *i2c_dev = NULL;
    uint8_t         buf[4];


    if( argc != 2)
    {
        printf("This program used to do I2C test by SHT20 sensor.\n");
        printf("Usage: %s /dev/i2c-x\n", argv[0]);
        return 0;
    }

    i2c_dev = argv[1];

    /*+--------------------------------+
     *|     open /dev/i2c-x device     |
     *+--------------------------------+*/
    if( (fd=open(i2c_dev, O_RDWR)) < 0)
    {
        printf("i2c device open failed: %s\n", strerror(errno));
        return -1;
    }

    /*+--------------------------------+
     *| set I2C mode and slave address |
     *+--------------------------------+*/
    ioctl(fd, I2C_TENBIT, 0);    /* Not 10-bit but 7-bit mode */
    ioctl(fd, I2C_SLAVE, 0x40);  /* set SHT2x slava address 0x40 */

    /*+--------------------------------+
     *|   software reset SHT20 sensor  |
     *+--------------------------------+*/

    buf[0] = 0xFE;
    write(fd, buf, 1);

    msleep(50);


    /*+--------------------------------+
     *|   trigger temperature measure  |
     *+--------------------------------+*/

    buf[0] = 0xF3;
    write(fd, buf, 1);

    msleep(85); /* datasheet: typ=66, max=85 */

    memset(buf, 0, sizeof(buf));
    read(fd, buf, 3);
    dump_buf("Temperature sample data: ", buf, 3);

    if( !sht20_checksum(buf, 2, buf[2]) )
    {
        printf("Temperature sample data CRC checksum failure.\n");
        goto cleanup;
    }

    temp = 175.72 * (((((int) buf[0]) << 8) + buf[1]) / 65536.0) - 46.85;


    /*+--------------------------------+
     *|    trigger humidity measure    |
     *+--------------------------------+*/

    buf[0] = 0xF5;
    write(fd, buf, 1);

    msleep(29); /* datasheet: typ=22, max=29 */

    memset(buf, 0, sizeof(buf));
    read(fd, buf, 3);
    dump_buf("Relative humidity sample data: ", buf, 3);

    if( !sht20_checksum(buf, 2, buf[2]) )
    {
        printf("Relative humidity sample data CRC checksum failure.\n");
        goto cleanup;
    }

    rh = 125 * (((((int) buf[0]) << 8) + buf[1]) / 65536.0) - 6;

    /*+--------------------------------+
     *|    print the measure result    |
     *+--------------------------------+*/

    printf("Temperature=%lf 'C relative humidity=%lf%%\n", temp, rh);

cleanup:
    close(fd);
    return 0;
}

int sht20_checksum(uint8_t *data, int len, int8_t checksum)
{
    int8_t crc = 0;
    int8_t bit;
    int8_t byteCtr;

    //calculates 8-Bit checksum with given polynomial: x^8 + x^5 + x^4 + 1
    for (byteCtr = 0; byteCtr < len; ++byteCtr)
    {
        crc ^= (data[byteCtr]);
        for ( bit = 8; bit > 0; --bit)
        {
            /* x^8 + x^5 + x^4 + 1 = 0001 0011 0001 = 0x131 */
            if (crc & 0x80) crc = (crc << 1) ^ 0x131;
            else crc = (crc << 1);
        }
    }

    if (crc != checksum)
        return 0;
    else
        return 1;
}

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
    return ;
}

static inline void dump_buf(const char *prompt, uint8_t *buf, int size)
{
    int          i;

    if( !buf )
    {
        return ;
    }

    if( prompt )
    {
        printf("%-32s ", prompt);
    }

    for(i=0; i<size; i++)
    {
        printf("%02x ", buf[i]);
    }
    printf("\n");

    return ;
}
