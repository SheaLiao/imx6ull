/*********************************************************************************
 *      Copyright:  (C) 2025 Liao Shengli<liaoshengli@gmail.com>
 *                  All rights reserved.
 *
 *       Filename:  spi_test.c
 *    Description:  This file 
 *                 
 *        Version:  1.0.0(01/21/2025)
 *         Author:  Liao Shengli <liaoshengli@gmail.com>
 *      ChangeLog:  1, Release initial version on "01/21/2025 12:11:47 PM"
 *                 
 ********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>


typedef struct spi_ctx_s
{
	int			fd;
	char		dev[64];
	uint8_t		bits;
	uint16_t	delay;
	uint32_t	mode;
	uint32_t	speed;
} spi_ctx_t;


static int spi_init(spi_ctx_t *spi_ctx);
static int transfer(spi_ctx_t *spi_ctx, uint8_t const *tx, uint8_t const *rx, size_t len);


static void program_usage(char *progname)
{
    printf("Usage: %s [OPTION]...\n", progname);
    printf(" %s is a program to test IGKBoard loop spi\n", progname);

    printf("\nMandatory arguments to long options are mandatory for short options too:\n");
    printf(" -d[device  ]  Specify SPI device, such as: /dev/spidev0.0\n");
    printf(" -s[speed   ]  max speed (Hz), such as: -s 500000\n");
    printf(" -p[print   ]  Send data (such as: -p 1234/xde/xad)\n");

    printf("\n%s version 1.0\n", progname);
    return;
}


int main (int argc, char **argv)
{
	spi_ctx_t	spi_ctx;
	uint32_t	spi_speed = 500000;
	char		*spi_dev = "/dev/spidev0.0";
	char		*input_tx = "hello, world";
	uint8_t		rx_buffer[128];
	int			opt;
	char		*progname = NULL;

	struct option long_options[] = {
        {"device", required_argument, NULL, 'd'},
        {"speed", required_argument, NULL, 's'},
        {"print", required_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    progname = (char *)basename(argv[0]);

    while((opt = getopt_long(argc, argv, "d:s:p:h", long_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 'd':
            spi_dev = optarg;
            break;

        case 's':
            spi_speed = atoi(optarg);
            break;

        case 'p':
            input_tx = optarg;
            break;

        case 'h':
            program_usage(progname);
            return 0;

        default:
            break;
        }
    }

	memset(&spi_ctx, 0, sizeof(spi_ctx));
	strncpy(spi_ctx.dev, spi_dev, sizeof(spi_ctx.dev));
	spi_ctx.bits = 8;
    spi_ctx.delay = 100;
    spi_ctx.mode = SPI_MODE_2;
    spi_ctx.speed = spi_speed;

	if( spi_init(&spi_ctx) < 0 )
    {
        printf("Initial SPI device '%s' failed.\n", spi_ctx.dev);
        return -1;
    }
    printf("Initial SPI device '%s' okay.\n", spi_ctx.dev);

    if ( transfer(&spi_ctx, input_tx, rx_buffer, strlen(input_tx)) < 0 )
    {
        printf("spi transfer error\n");
        return -2;
    }

    printf("tx_buffer: | %s |\n", input_tx);
    printf("rx_buffer: | %s |\n", rx_buffer);

	return 0;
} 


int spi_init(spi_ctx_t *spi_ctx)
{
	int		ret;
	spi_ctx->fd = open(spi_ctx->dev, O_RDWR);

	if(spi_ctx->fd < 0)
	{
		printf("open %s error\n", spi_ctx->dev);
		return -1;
	}

	ret = ioctl(spi_ctx->fd, SPI_IOC_RD_MODE, &spi_ctx->mode);
	if(ret < 0)
	{
		printf("ERROR: SPI set SPI_IOC_RD_MODE [0x%x] failure: %s\n ", spi_ctx->mode, strerror(errno));
        goto cleanup;
	}

	ret = ioctl(spi_ctx->fd, SPI_IOC_WR_MODE, &spi_ctx->mode);
    if( ret < 0 )
    {
        printf("ERROR: SPI set SPI_IOC_WR_MODE [0x%x] failure: %s\n ", spi_ctx->mode, strerror(errno));
        goto cleanup;
    }

    ret = ioctl(spi_ctx->fd, SPI_IOC_RD_BITS_PER_WORD, &spi_ctx->bits);
    if( ret < 0 )
    {
        printf("ERROR: SPI set SPI_IOC_RD_BITS_PER_WORD [%d] failure: %s\n ", spi_ctx->bits, strerror(errno));
        goto cleanup;
    }

	ret = ioctl(spi_ctx->fd, SPI_IOC_WR_BITS_PER_WORD, &spi_ctx->bits);
    if( ret < 0 )
    {
        printf("ERROR: SPI set SPI_IOC_WR_BITS_PER_WORD [%d] failure: %s\n ", spi_ctx->bits, strerror(errno));
        goto cleanup;
    }

    ret = ioctl(spi_ctx->fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_ctx->speed);
    if( ret == -1)
    {
        printf("ERROR: SPI set SPI_IOC_WR_MAX_SPEED_HZ [%d] failure: %s\n ", spi_ctx->speed, strerror(errno));
        goto cleanup;
    }

	ret = ioctl(spi_ctx->fd, SPI_IOC_RD_MAX_SPEED_HZ, &spi_ctx->speed);
    if( ret == -1)
    {
        printf("ERROR: SPI set SPI_IOC_RD_MAX_SPEED_HZ [%d] failure: %s\n ", spi_ctx->speed, strerror(errno));
        goto cleanup;
    }

    printf("spi mode: 0x%x\n", spi_ctx->mode);
    printf("bits per word: %d\n", spi_ctx->bits);
    printf("max speed: %d Hz (%d KHz)\n", spi_ctx->speed, spi_ctx->speed / 1000);

   return spi_ctx->fd;

cleanup:
   close(spi_ctx->fd);
   return -1;
}



int transfer(spi_ctx_t *spi_ctx, uint8_t const *tx, uint8_t const *rx, size_t len)
{
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len,
		.delay_usecs = spi_ctx->delay,
		.speed_hz = spi_ctx->speed,
		.bits_per_word = spi_ctx->bits,
	};

	if(ioctl(spi_ctx->fd, SPI_IOC_MESSAGE(1), &tr) < 0)
	{
		printf("ERROR: SPI transfer failure: %s\n ", strerror(errno));
        return -1;
	}

	return 0;
}
