/*********************************************************************************
 *      Copyright:  (C) 2025 Liao Shengli<liaoshengli@gmail.com>
 *                  All rights reserved.
 *
 *       Filename:  ttyS_test.c
 *    Description:  This file 
 *                 
 *        Version:  1.0.0(01/21/2025)
 *         Author:  Liao Shengli <liaoshengli@gmail.com>
 *      ChangeLog:  1, Release initial version on "01/21/2025 03:43:59 PM"
 *                 
 ********************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <pthread.h>

#define DEV_NAME    "/dev/ttymxc1"
#define MSG			"hello, world!"


int main (int argc, char **argv)
{
	struct termios	tty;
	int				serial_fd;
	int				bytes;
	char			read_buf[64];

	serial_fd = open(DEV_NAME, O_RDWR);
	if(serial_fd < 0)
	{
		printf("Open '%s' failed: %s\n", DEV_NAME, strerror(errno));
        return 0;
	}

	tcgetattr(serial_fd, &tty);

	tty.c_cflag &= ~PARENB;    // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB;    // Clear stop field, only one stop bit used in communication (most common)
    tty.c_cflag &= ~CSIZE;     // Clear all bits that set the data size
    tty.c_cflag |= CS8;        // 8 bits per byte (most common)
    tty.c_cflag &= ~CRTSCTS;   // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;      // Disable echo
    tty.c_lflag &= ~ECHOE;     // Disable erasure
    tty.c_lflag &= ~ECHONL;    // Disable new-line echo
    tty.c_lflag &= ~ISIG;      // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST;     // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR;     // Prevent conversion of newline to carriage return/line feed

    tty.c_cc[VTIME] = 10;      // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;

    /* Set in/out baud rate to be 115200 */
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    tcsetattr(serial_fd, TCSANOW, &tty);

	/*+-------------------------+
      |   Write to Serial Port  |
      +-------------------------+*/
    write(serial_fd, MSG, strlen(MSG));
    printf("Send MSG    : %s\n", MSG);

    /*+-------------------------+
      |  Read from Serial Port  |
      +-------------------------+*/
    /*
     * The behaviour of read() (e.g. does it block?, how long does it block for?)
     * depends on the configuration settings above, specifically VMIN and VTIME
     */
    memset(&read_buf, '\0', sizeof(read_buf));
    bytes =  read(serial_fd, &read_buf, sizeof(read_buf));
    if (bytes< 0)
    {
        printf("Error reading: %s", strerror(errno));
        goto cleanup;
    }

    printf("Received MSG: %s\n", read_buf);

    /*+-------------------------+
      |   close Serial Port     |
      +-------------------------+*/

cleanup:
    close(serial_fd);
    return 0;
} 

