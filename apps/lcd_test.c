/*********************************************************************************
 *      Copyright:  (C) 2025 Liao Shengli<liaoshengli@gmail.com>
 *                  All rights reserved.
 *
 *       Filename:  lcd_test.c
 *    Description:  This file 
 *                 
 *        Version:  1.0.0(02/09/2025)
 *         Author:  Liao Shengli <liaoshengli@gmail.com>
 *      ChangeLog:  1, Release initial version on "02/09/2025 02:13:45 PM"
 *                 
 ********************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <sys/mman.h>


/*程序版本*/
#define PROG_VERSION      "1.0.0"

/*RGB565 颜色*/
/*颜色对照表 https://blog.csdn.net/weixin_45020839/article/details/117781108 */
#define RED     0xF800       //红色
#define GREED   0x07E0       //绿色
#define BLUE    0x001f       //蓝色
#define WTITE   0xFFFF       //白色
#define BLACK   0x0000       //黑色


enum
{
	CMD_SHOW_INFO,
	CMD_SHOW_RGB,
	CMD_SHOW_BMP,
};


typedef struct fb_ctx_s
{
	int							fd;
	char						dev[64];
	long						fb_size;
	int							pix_size;
	struct fb_var_screeninfo	vinfo;
	char						*fbp;
} fb_ctx_t;


typedef struct bmp_file_head_s
{
    uint8_t  bfType[2];        /* BMP file type: "BM" */
    int32_t bfSize;           /* BMP file size */
    int32_t bfReserved;       /* reserved */
    int32_t bfoffBits;        /* image data offset */
}__attribute__((packed)) bmp_file_head_t;
//__attribute__((packed))的作用是告诉编译器取消结构在编译过程中的优化对齐

typedef struct bmp_bitmap_info_s
{
    int32_t biSize;           /* this struture size */
    int32_t biWidth;          /* image width in pix */
    int32_t biHeight;         /* image height in pix */
    uint16_t biPlanes;         /* display planes, always be 1 */
    uint16_t biBitCount;       /* bpp: 1,4,8,16,24,32 */
    int32_t biCompress;       /* compress type */
    int32_t biSizeImage;      /* image size in byte */
    int32_t biXPelsPerMeter;  /* x-res in pix/m */
    int32_t biYPelsPerMeter;  /* y-res in pix/m */
    int32_t biClrUsed;
    int32_t biClrImportant;
}__attribute__((packed)) bmp_bitmap_info_t;


int fb_init(fb_ctx_t *fb_ctx);
int fb_term(fb_ctx_t *fb_ctx);
int lcd_show_pixel_rgb565(fb_ctx_t *fb_ctx, unsigned int x, unsigned int y, unsigned short color);
int lcd_fill_rgb565(fb_ctx_t *fb_ctx, unsigned short color);
int show_example_line_fill(fb_ctx_t *fb_ctx, int times);
int show_bmp(fb_ctx_t *fb_ctx, char *bmp_file);


static void program_usage(char *progname)
{
	printf("Usage: %s [OPTION]...\n", progname);
	printf(" %s is a program to show RGB color or BMP file on LCD screen\n", progname);

	printf("\nMandatory arguments to long options are mandatory for short options too:\n");
	printf(" -d[device  ]  Specify framebuffer device, such as: /dev/fb0\n");
	printf(" -e[example ]  Display RGB corlor and draw rand line on LCD screen for some times, such as: -c 3\n");
	printf(" -b[bmp     ]  Display BMP file, such as: -b bg.bmp\n");
	printf(" -h[help    ]  Display this help information\n");
	printf(" -v[version ]  Display the program version\n");

	printf("\n%s version %s\n", progname, PROG_VERSION);
	return;
}



int main (int argc, char **argv)
{
	fb_ctx_t	fb_ctx;
	char		*progname = NULL;
	char		*bmp_file = NULL;
	char		*fb_dev = "/dev/fb0";
	int			cmd = CMD_SHOW_INFO;
	int			opt, times;

	struct option long_options[] = {
		{"device", required_argument, NULL, 'd'},
		{"example", required_argument, NULL, 'c'},
		{"bmp", required_argument, NULL, 'b'},
		{"version", no_argument, NULL, 'v'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	progname = (char *)basename(argv[0]);

	while ((opt = getopt_long(argc, argv, "d:c:b:vh", long_options, NULL)) != -1)
    {
        switch (opt)
        {
            case 'd': /* Set framebuffer device */
                 fb_dev = optarg;
                break;

            case 'b': /* Set BMP file */
                /* 设置bmp 图片位置 */
                bmp_file = optarg;
                cmd = CMD_SHOW_BMP;
                break;

            case 'c': /* Show RGB color */
                /* 设置RGB颜色循环次数 */
                times = atoi(optarg);
                cmd = CMD_SHOW_RGB;
                break;

            case 'v':  /* Get software version */
                printf("%s version %s\n", progname, PROG_VERSION);
                return 0;

            case 'h':  /* Get help information */
                program_usage(progname);
                return 0;

            default:
                break;
        }

    }

	memset(&fb_ctx, 0, sizeof(fb_ctx));
	strncpy(fb_ctx.dev, fb_dev, sizeof(fb_ctx.dev));

	if( fb_init(&fb_ctx) < 0 )
    {
        printf("ERROR: Initial framebuffer device '%s' failure.\n", fb_ctx.dev);
        return 1;
    }

	switch( cmd )
	{
		case CMD_SHOW_RGB:
            show_example_line_fill(&fb_ctx, times);
            break;

        case CMD_SHOW_BMP:
			show_bmp(&fb_ctx, bmp_file);
            break;

        default:
            break;
	}

	fb_term(&fb_ctx);

	return 0;
} 


int fb_get_var_screeninfo(int fd, struct fb_var_screeninfo *vinfo)
{
	if( fd < 0 || !vinfo )
	{
		printf("ERROR: Invalid input arguments\n");
        return -1;
	}

	if( ioctl(fd, FBIOGET_VSCREENINFO, vinfo) )
	{
		printf("ERROR: ioctl() get variable info failure: %s\n", strerror(errno));
        return -2;
	}

	printf("LCD information : %dx%d, bpp:%d rgba:%d/%d,%d/%d,%d/%d,%d/%d\n", vinfo->xres, vinfo->yres, vinfo->bits_per_pixel,
            vinfo->red.length, vinfo->red.offset, vinfo->green.length, vinfo->green.offset,
            vinfo->blue.length,vinfo->blue.offset, vinfo->transp.length, vinfo->transp.offset);

    return 0;
}


int fb_init(fb_ctx_t *fb_ctx)
{
	struct fb_var_screeninfo	*vinfo;

	if( !fb_ctx || !strlen(fb_ctx->dev) )
    {
        printf("ERROR: Invalid input arguments\n");
        return -1;
    }

	if( (fb_ctx->fd = open(fb_ctx->dev, O_RDWR)) < 0 )
	{
		printf("ERROR: Open framebuffer device '%s' failure: %s\n", fb_ctx->dev, strerror(errno));
        return -2;
	}

	fb_get_var_screeninfo(fb_ctx->fd, &fb_ctx->vinfo);

	vinfo = &fb_ctx->vinfo;
	fb_ctx->fb_size = vinfo->xres * vinfo->yres * vinfo->bits_per_pixel / 8;
    fb_ctx->pix_size = vinfo->xres * vinfo->yres;

	fb_ctx->fbp =(char *)mmap(0, fb_ctx->fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_ctx->fd ,0);
    if ( !fb_ctx->fbp )
    {
        printf("ERROR: Framebuffer mmap() failure: %s\n", strerror(errno));
        return -2;
    }

    vinfo->xoffset = 0;
    vinfo->yoffset = 0;

    return 0;
}

int fb_term(fb_ctx_t *fb_ctx)
{
	if( !fb_ctx )
    {
        printf("ERROR: Invalid input arguments\n");
        return -1;
    }

	munmap(fb_ctx->fbp, fb_ctx->fb_size);
	close(fb_ctx->fd);

	return 0;
}


int lcd_show_pixel_rgb565(fb_ctx_t *fb_ctx, unsigned int x, unsigned int y, unsigned short color)
{
	char	*color_addr;

	if( !fb_ctx || x > fb_ctx->vinfo.xres || y > fb_ctx->vinfo.yres)
    {
        printf("ERROR: Invalid input arguments\n");
        return -1;
    }

	/*计算点的实际地址 起始地址 + (y*一行像素点数量 + x)*Bpp  */
    color_addr = fb_ctx->fbp + (fb_ctx->vinfo.xres * y + x) * (fb_ctx->vinfo.bits_per_pixel / 8);
    memcpy(color_addr, &color, fb_ctx->vinfo.bits_per_pixel / 8);
    return 0;
}


int lcd_fill_rgb565(fb_ctx_t *fb_ctx, unsigned short color)
{
	struct fb_var_screeninfo	*vinfo;
	int		i;
	int		bpp_bytes;
	char	*fb_addr;

	if( !fb_ctx )
    {
        printf("ERROR: Invalid input arguments\n");
        return -1;
    }

	vinfo = &fb_ctx->vinfo;
	//rgb565 = 16 / 8 = 2
    bpp_bytes = vinfo->bits_per_pixel / 8;
    fb_addr = fb_ctx->fbp;
     /* 循环次数=像素点数量 */
    for(i=0; i<fb_ctx->pix_size; i++)
    {
        //对显存映射内存进行赋值，颜色为红色，2个字节进行拷贝
        memcpy(fb_addr, &color, bpp_bytes);
        //移动赋值地址 偏移为2byte
        fb_addr += bpp_bytes;
    }
    return 0;
}


int show_example_line_fill(fb_ctx_t *fb_ctx, int times)
{
    int                     i;
    int                     j;
    unsigned int     line_y=0;
    if( !fb_ctx )
    {
        printf("ERROR: Invalid input arguments\n");
        return -1;
    }

    lcd_fill_rgb565(fb_ctx, WTITE);
    lcd_fill_rgb565(fb_ctx, RED);
    sleep(1);
    lcd_fill_rgb565(fb_ctx, BLUE);
    sleep(1);
    lcd_fill_rgb565(fb_ctx, GREED);
    sleep(1);
    lcd_fill_rgb565(fb_ctx, WTITE);

    for(i=0; i<times; i++)
    {
        line_y = rand()%(fb_ctx->vinfo.yres - 0 + 1) + 0;
        printf("draw line [y=%d]\n", line_y);
        for(j=0; j<200; j++)
        {
             lcd_show_pixel_rgb565(fb_ctx, fb_ctx->vinfo.xres/2+j, line_y, BLACK);
        }
    }
    return 0;
}


int show_bmp(fb_ctx_t *fb_ctx, char *bmp_file)
{
	struct fb_var_screeninfo    *vinfo;
    char                        *fb_addr;
    bmp_file_head_t            *file_head;
    bmp_bitmap_info_t          *bitmap_info;
    struct stat                 statbuf;
    char                       *f_addr;
    char                       *d_addr;
    char                       *p_addr;
    int                         fd;
    int                         bpp_bytes;
    int                         i, j;
    int                         width, height;

    if( !fb_ctx || !bmp_file )
    {
        printf("ERROR: Invalid input arguments\n");
        return -1;
    }

	if( (fd = open(bmp_file, O_RDONLY)) < 0 )
	{
		printf("ERROR: Open file '%s' failure: %s\n", bmp_file, strerror(errno));
        return -2;
	}

	fstat(fd, &statbuf);

	f_addr = (char *)mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if ( !f_addr )
    {
        printf("ERROR: BMP file mmap() failure: %s\n", strerror(errno));
        return -2;
    }

	file_head = (bmp_file_head_t *)f_addr;
	/* 确定前2字节是否符合类型 */
    if( memcmp(file_head->bfType, "BM", 2) != 0)
    {
        printf("ERROR: It's not a BMP file\n");
        return -3;
    }

	bitmap_info = (bmp_bitmap_info_t *)(f_addr + sizeof(bmp_file_head_t));

	d_addr = f_addr + file_head->bfoffBits;/* 利用bmp文件头的数据偏移数据确定实际位图数据地址 */

	vinfo = &fb_ctx->vinfo;
	fb_addr = fb_ctx->fbp;
	bpp_bytes = vinfo->bits_per_pixel / 8;

	/* 控制图片大小小于LCD尺寸 */
    width = bitmap_info->biWidth>vinfo->xres ? vinfo->xres : bitmap_info->biWidth;
    height = bitmap_info->biHeight>vinfo->yres ? vinfo->yres : bitmap_info->biHeight;

	 printf("BMP file '%s': %dx%d and display %dx%d\n", bmp_file, bitmap_info->biWidth, bitmap_info->biHeight, width, height);

	 /* if biHeight is positive, the bitmap is a bottom-up DIB */
    for(i=0; i<height; i++)
    {
        //由于biHeight为正数，则位图数据排布方式自底至上
        /* 按照一行一行图片进行内存拷贝
         * 第一行显示的数据，为位图数据的最后一行数据
         * 最后一行位图数据的偏移为 (height-1)*bitmap_info->biWidth*bpp_bytes
         */
        p_addr=d_addr+(height-1-i)*bitmap_info->biWidth*bpp_bytes;
        memcpy(fb_addr, p_addr, bpp_bytes*width);
        fb_addr += bpp_bytes * vinfo->xres;
    }

    munmap(f_addr, statbuf.st_size);
    close (fd);

    return 0;
}


