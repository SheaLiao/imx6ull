/*********************************************************************************
 *      Copyright:  (C) 2025 Liao Shengli<liaoshengli@gmail.com>
 *                  All rights reserved.
 *
 *       Filename:  video3lcd.c
 *    Description:  This file 
 *                 
 *        Version:  1.0.0(02/14/2025)
 *         Author:  Liao Shengli <liaoshengli@gmail.com>
 *      ChangeLog:  1, Release initial version on "02/14/2025 08:26:39 PM"
 *                 
 ********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>

#define Y0              0
#define U               1
#define Y1              2
#define V               3

#define R               0
#define G               1
#define B               2

/* 注，屏幕尺寸要大于帧的尺寸 */
#define FRAME_WIDTH     640
#define FRAME_HEIGH     480

#define LCD_WIDTH       800
#define LCD_HEIGH       480


/* 所申请的单个 buffer 结构体
 * 包含 buffer 的起始地址和长度
 */
struct v4l2_buffer_unit {
    void           *start;
    size_t         length;
};

int                        fd_camera = -1;
int                        fd_lcd = -1;
void                       *screen_base = NULL;
struct v4l2_buffer_unit    *buffer_unit = NULL;
void                       *rgb_buffer = NULL;

/* yuv 格式转为 rgb 格式的算法
 * 将 yuv422 格式的帧数据转换为 rgb565 格式，以显示在 lcd 屏幕上
 */
int yuv422_rgb565(unsigned char *yuv_buf, unsigned char *rgb_buf, unsigned int width, unsigned int height)
{
    int              yuvdata[4];
    int              rgbdata[3];
    unsigned char    *rgb_temp;
    unsigned int     i, j;

    rgb_temp = rgb_buf;
    for (i = 0; i < height * 2; i++)
    {
        for (j = 0; j < width; j += 4)
        {
            /* get Y0 U Y1 V */
            yuvdata[Y0] = *(yuv_buf + i * width + j + 0);
            yuvdata[U]  = *(yuv_buf + i * width + j + 1);
            yuvdata[Y1] = *(yuv_buf + i * width + j + 2);
            yuvdata[V]  = *(yuv_buf + i * width + j + 3);

            /* the first pixel */
            rgbdata[R] = yuvdata[Y0] + (yuvdata[V] - 128) + (((yuvdata[V] - 128) * 104 ) >> 8);
            rgbdata[G] = yuvdata[Y0] - (((yuvdata[U] - 128) * 89) >> 8) - (((yuvdata[V] - 128) * 183) >> 8);
            rgbdata[B] = yuvdata[Y0] + (yuvdata[U] - 128) + (((yuvdata[U] - 128) * 199) >> 8);

            if (rgbdata[R] > 255)  rgbdata[R] = 255;
            if (rgbdata[R] < 0) rgbdata[R] = 0;
            if (rgbdata[G] > 255)  rgbdata[G] = 255;
            if (rgbdata[G] < 0) rgbdata[G] = 0;
            if (rgbdata[B] > 255)  rgbdata[B] = 255;
            if (rgbdata[B] < 0) rgbdata[B] = 0;

            *(rgb_temp++) =( ((rgbdata[G]& 0x1C) << 3) | (rgbdata[B] >> 3) );
            *(rgb_temp++) =( (rgbdata[R]& 0xF8) | (rgbdata[G] >> 5) );

            /* the second pixel */
            rgbdata[R] = yuvdata[Y1] + (yuvdata[V] - 128) + (((yuvdata[V] - 128) * 104 ) >> 8);
            rgbdata[G] = yuvdata[Y1] - (((yuvdata[U] - 128) * 89) >> 8) - (((yuvdata[V] - 128) * 183) >> 8);
            rgbdata[B] = yuvdata[Y1] + (yuvdata[U] - 128) + (((yuvdata[U] - 128) * 199) >> 8);

            if (rgbdata[R] > 255)  rgbdata[R] = 255;
            if (rgbdata[R] < 0) rgbdata[R] = 0;
            if (rgbdata[G] > 255)  rgbdata[G] = 255;
            if (rgbdata[G] < 0) rgbdata[G] = 0;
            if (rgbdata[B] > 255)  rgbdata[B] = 255;
            if (rgbdata[B] < 0) rgbdata[B] = 0;

            *(rgb_temp++) =( ((rgbdata[G]& 0x1C) << 3) | (rgbdata[B] >> 3) );
            *(rgb_temp++) =( (rgbdata[R]& 0xF8) | (rgbdata[G] >> 5) );

        }
    }

    return 0;
}

/* 打开屏幕和摄像头的设备节点，并映射 lcd 的用户空间到内核空间 */
int init_camera_lcd()
{
    fd_camera = open("/dev/video2", O_RDWR | O_NONBLOCK, 0);
    if(fd_camera < 0)
    {
        printf("%s : open camera error\n", __FUNCTION__);
        return -1;
    }

    fd_lcd = open("/dev/fb0", O_RDWR);
    if(fd_lcd < 0)
    {
        printf("%s : open lcd error\n", __FUNCTION__);
        return -2;
    }

    screen_base = mmap(NULL, LCD_WIDTH*LCD_HEIGH*2, PROT_READ|PROT_WRITE, MAP_SHARED, fd_lcd, 0);
    if(NULL == screen_base)
    {
        printf("%s : framebuffer mmap error\n", __FUNCTION__);
        return -3;
    }

    rgb_buffer = malloc(FRAME_WIDTH*FRAME_HEIGH*2);

    memset(screen_base, 0x0, LCD_WIDTH*LCD_HEIGH*2);

    return 0;
}

/* 查询设备功能属性 */
int v4l2_query_capability()
{
    struct v4l2_capability     cap;
    int                        ret;

    ret = ioctl(fd_camera, VIDIOC_QUERYCAP, &cap);
    if(ret < 0)
    {
        printf("%s : VIDIOC_QUERYCAP error\n", __FUNCTION__);
        return -1;
    }

    /* #define V4L2_CAP_VIDEO_CAPTURE 0x00000001
    * 获取到设备的能力
    * 判断最低位是否为 1，如果为 1，则说明该设备具有视频采集能力
    */
    if ((V4L2_CAP_VIDEO_CAPTURE & cap.capabilities) == 0x00)
    {
        printf("%s : no capture device\n", __FUNCTION__);
        return -2;
    }

    return 0;
}

/* 列举设备支持的数据格式 */
int v4l2_enum_format()
{
    int                     ret = 0;
    int                     found = 0;
    struct v4l2_fmtdesc     fmtdesc;

    /* 枚举摄像头所支持的所有像素格式以及描述信息 */
    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while(found == 0 || ret == 0)
    {
        ret = ioctl(fd_camera, VIDIOC_ENUM_FMT, &fmtdesc);

        if(fmtdesc.pixelformat = V4L2_PIX_FMT_YUYV)
        {
            found = 1;
        }

        fmtdesc.index++;

    }

    if(found != 1)
    {
        printf("%s : device don't support V4L2_PIX_FMT_YUYV\n", __FUNCTION__);
        return -1;
    }
    else
    {
        printf("device support V4L2_PIX_FMT_YUYV\n");
    }

    return 0;
}

/* 获取帧数据格式
 * 主要获取帧数据的宽和高用于之后的设置
 */
void v4l2_get_format()
{
    int                  ret;
    struct v4l2_format   format;

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ret = ioctl(fd_camera, VIDIOC_G_FMT, &format);
    if(ret < 0)
    {
        printf("%s : VIDIOC_G_FMT error\n", __FUNCTION__);
    }

    printf("width:%d height:%d\n", format.fmt.pix.width, format.fmt.pix.height);
}

/* 获取帧数据格式 */
void v4l2_set_format()
{
    int                   ret;
    struct v4l2_format    format;

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = FRAME_WIDTH;
    format.fmt.pix.height = FRAME_HEIGH;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    format.fmt.pix.field = V4L2_FIELD_INTERLACED;

    ret = ioctl(fd_camera, VIDIOC_S_FMT, &format);
    if(ret <  0)
    {
        printf("%s : VIDIOC_S_FMT error\n", __FUNCTION__);
    }
}

/* 申请帧数据缓冲区 */
void v4l2_require_buffer()
{
    int                         ret;
    struct v4l2_requestbuffers  req;

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(fd_camera, VIDIOC_REQBUFS, &req);
    if(ret < 0)
    {
        printf("%s : VIDIOC_REQBUFS error\n", __FUNCTION__);
    }
}

/* 查询申请到的 buffer 信息，并映射到用户空间 */
int v4l2_query_buffer()
{
    int                  ret;
    int                  count;
    struct v4l2_buffer   buf;

    /* 分配 4 个大小为 v4l2_buffer_unit 结构体大小的 buffer */
    buffer_unit = calloc(4, sizeof(*buffer_unit));
    if(!buffer_unit)
    {
        printf("%s : calloc buffer_unit error\n", __FUNCTION__);
    }

    /* 获取之前申请的 v4l2_requestbuffers 的信息
     * 并将这些信息传给用户空间的 buffer_unit
     * 然后将 buffer_unit 映射到内核中申请的 buffer 上去
     * 映射过程是按照，申请 buffer 的编号一个一个映射
     */
    for(count=0; count<4; count++)
    {
        memset(&buf,0,sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_BUF_FLAG_MAPPED;
        buf.index = count;

        ret = ioctl(fd_camera, VIDIOC_QUERYBUF, &buf);
        if(ret < 0)
        {
            printf("%s : VIDIOC_QUERYBUF error\n", __FUNCTION__);
            return -1;
        }

        buffer_unit[count].length = buf.length;
        buffer_unit[count].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_camera, buf.m.offset);

        if (NULL == buffer_unit[count].start)
        {
            printf("%s : mmap buffer_unit error\n", __FUNCTION__);
            return -2;
        }

    }

    return 0;
}

/* 开始视频数据采集，并将 buffer 入队 */
int v4l2_stream_on()
{
    int                     i;
    int                     ret;
    enum  v4l2_buf_type     type;
    struct v4l2_buffer      buffer;

    for(i=0; i<4; i++)
    {
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        ret=ioctl(fd_camera, VIDIOC_QBUF, &buffer);
        if(ret < 0)
        {
            printf("%s : VIDIOC_QBUF error\n", __FUNCTION__);
            return -1;
        }

    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd_camera, VIDIOC_STREAMON, &type);
    if(ret < 0)
    {
        printf("%s : VIDIOC_STREAMON error\n", __FUNCTION__);
        return -2;
    }

    return 0;
}

/* 从帧缓冲队列中取出一个缓冲区数据，并显示到屏幕上 */
int v4l2_dequeue_buffer()
{
    int                  ret;
    int                  i;
    struct v4l2_buffer   buffer;
    unsigned short       *base;
    unsigned short       *start;

    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(fd_camera, VIDIOC_DQBUF, &buffer);
    if(ret < 0)
    {
        printf("%s : VIDIOC_STREAMON error\n", __FUNCTION__);
        return -1;
    }

    /* 如果 buffer.index < 4 为假，则会打印
    * 因为，之前只开辟了 4 个 buffer_uint，编号从 0 到 3
    */
    assert(buffer.index < 4);

    /* 将数据转换格式后，放到屏幕申请 frame buffer 的空间中 */
    yuv422_rgb565(buffer_unit[buffer.index].start, rgb_buffer, FRAME_WIDTH, FRAME_HEIGH);

    for (i=0, base=screen_base, start=rgb_buffer; i<FRAME_HEIGH; i++)
    {
        memcpy(base, start, FRAME_WIDTH * 2);     // RGB565 一个像素占 2 个字节
        base += LCD_WIDTH;                        // lcd 显示指向下一行
        start += FRAME_WIDTH;                     // 指向下一行数据
    }
    memset(rgb_buffer, 0x0, FRAME_WIDTH*FRAME_HEIGH*2);

    ioctl(fd_camera, VIDIOC_QBUF, &buffer);

    return 0;
}

/* 用 select 监听数据
 * 有数据到来，就从缓冲队列中取出数据
 */
int v4l2_select()
{
    while(1)
    {
        int                  ret;
        struct timeval       tv;
        fd_set               fds;

        tv.tv_sec = 2;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(fd_camera, &fds);

        ret = select(fd_camera+1, &fds, NULL, NULL, &tv);
        if(ret < 0)
        {
            if(errno = EINTR)
            {
                printf("%s : select error\n", __FUNCTION__);
            }
            return -1;
        }
        if(0 == ret)
        {
            printf("%s : select timeout\n", __FUNCTION__);
            return -2;
        }

        v4l2_dequeue_buffer();
    }

    return 0;
}

/* 结束视频数据采集 */
int v4l2_stream_off()
{
    int                   ret;
    enum v4l2_buf_type    type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd_camera, VIDIOC_STREAMOFF, &type);
    if(ret < 0)
    {
        printf("%s : VIDIOC_STREAMOFF error\n", __FUNCTION__);
        return -1;
    }

    return 0;
}

/* 解除 buffer_unit 到内核中申请 buffer 的映射 */
void v4l2_unmmap()
{
    int         i;
    int         ret;

    free(rgb_buffer);

    for(i=0; i<4; i++)
    {
        ret = munmap(buffer_unit[i].start, buffer_unit[i].length);
        if (ret < 0)
        {
            printf("%s : munmap error\n", __FUNCTION__);
        }
    }
}

int main(int argc, char **argv)
{
    init_camera_lcd();

    /* 获取设备的能力和帧数据格式，并设置数据格式 */
    v4l2_query_capability();
    v4l2_enum_format();
    v4l2_set_format();
    v4l2_get_format();

    /* 在内核中申请帧缓冲区
     * 并获取缓冲区信息，用以将用户空间的内存映射到内核空间
     */
    v4l2_require_buffer();
    v4l2_query_buffer();

    /* 开始视频数据采集
     * 使用 select 监听数据，有数据则从帧缓冲队列取出数据
     * 采集结束后，停止采集，并解除映射，关闭文件描述符
     */
    v4l2_stream_on();
    v4l2_select();
    v4l2_stream_off();
    v4l2_unmmap();

    close(fd_camera);
    close(fd_lcd);

    return 0;
}
