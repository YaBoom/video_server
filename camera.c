#include "camera.h"
#include "global.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <jpeglib.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <pthread.h>
#include <unistd.h>


static struct camera_t *camera_data;
static int n_buffer = 0;



//初始化视频设备.
int init_camera(const char *dev_path)
{
    //打开设备.
    int fd = open(dev_path, O_RDWR|O_NONBLOCK);
    if(0 > fd){
        perror("open dev failure!");
        return -1;
    }

    //查询相机能力.
    struct v4l2_capability cap;       //存储设备信息的结构体
    if(0 > ioctl(fd, VIDIOC_QUERYCAP, &cap)){ // fd：打开设备返回的文件句柄.
        perror("VIDIOC_QUERYCAP failure!");
        return -1;
    }

	//判断是否是一个视频捕捉设备.
    if (!(V4L2_CAP_VIDEO_CAPTURE & cap.capabilities)){
        perror("camera can`t CAPTURE!");
        return -1;
    }

	//判断是否支持视频流形式.
	if(!(V4L2_CAP_STREAMING & cap.capabilities)) {
		printf("The Current device does not support streaming i/o\n");
		return -1;
	}
    struct v4l2_fmtdesc fmt; //存储帧描述信息的结构体
    fmt.index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while(0 == ioctl(fd, VIDIOC_ENUM_FMT, &fmt)){
        printf("pixel format:%c%c%c%c, description:%s\n",
               fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,
               (fmt.pixelformat >> 16) & 0xFF, (fmt.pixelformat >> 24) & 0xFF,
               fmt.description);
        ++fmt.index;
	}

    //设置相机帧格式.
    struct v4l2_format format; //帧格式相关参数.
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	format.fmt.pix.width = IMG_WIDTH;
	format.fmt.pix.height = IMG_HEIGHT;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	format.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if(0 > ioctl(fd, VIDIOC_S_FMT, &format)){
		perror("VIDIOC_S_FMT failure!");
        return -1;
    }

	return fd;
}

int init_mmap(int fd)
{
    //向驱动申请内存缓冲区（内核空间）；
    struct v4l2_requestbuffers reqbuf;
    reqbuf.count = 4;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    if(0 > ioctl(fd, VIDIOC_REQBUFS, &reqbuf)){
        perror("VIDIOC_REQBUFS failure!");
        return -1;
    }

	//printf("n_buffer = %d\n", n_buffer);

    camera_data = calloc(reqbuf.count, sizeof(struct camera_t));
	if(camera_data == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

    //将内核缓冲区映射到用户空间（先查询后映射）；
    for (int i = 0; i < reqbuf.count; i++)
    {
        struct v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(0 > ioctl(fd, VIDIOC_QUERYBUF, &buf)){ //查询申请到缓冲区的信息.
            perror("VIDIOC_QUERYBUF failure!");
            return -1;
        }

        camera_data[i].length = buf.length;
        camera_data[i].start = mmap(NULL, buf.length,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED, fd, buf.m.offset);
    }

    //将申请的内核缓冲区放入输入队列中；
    for (int i = 0; i < reqbuf.count; ++i)
    {
        struct v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if(0 > ioctl(fd, VIDIOC_QBUF, &buf)){
            perror("VIDIOC_QBUF failure");
            return -1;
        }
    }
    n_buffer = reqbuf.count;
    return 0;
}

void *start_capturing(void *arg)
{
    // 开始采集数据.
    int fd = (int)arg;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(0 > ioctl(fd, VIDIOC_STREAMON, &type)) {
		perror("VIDIOC_STREAMON failure!");
		return NULL;
	}

	fd_set fds;
    struct timeval tv = {
        .tv_sec = 5,
        .tv_usec = 0,
    };
	while(global.capture) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        int r = select(fd+1, &fds, NULL, NULL, &tv);
        if(-1 == r) {
            if(EINTR == errno)
                continue;

            perror("Fail to select");
            break;
        }

        if(0 == r) {
            //fprintf(stderr, "select Timeout\n");
            continue;
        }

        if(read_frame(fd))
            continue;
	}

	pthread_exit(NULL);
}
int read_frame(int fd)
{
    struct v4l2_buffer buf;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if(0 > ioctl(fd, VIDIOC_DQBUF, &buf)){
        perror("VIDIOC_DQBUF failure!");
        return -1;
    }

	assert(buf.index < n_buffer);

	pthread_mutex_lock(&global.update_lock);

	//读取到全局的buffer中去.
	global.length = camera_data[buf.index].length;

	//采集的数据大于预定义的大小, 扩充内存.
	if(global.length > PICTURE_SIZE) {
		global.start = realloc(global.start, global.length);
		memcpy(global.start, camera_data[buf.index].start, global.length);
	} 
    else {
		memcpy(global.start, camera_data[buf.index].start, global.length);
	}

    // printf("pthread_cond_broadcast\n");
    pthread_mutex_unlock(&global.update_lock);

    //唤醒等待的线程.
	pthread_cond_broadcast(&global.update_cond);

	if(-1 == ioctl(fd, VIDIOC_QBUF, &buf)) {
		perror("Fail to ioctl 'VIDIOC_QBUF'");
		return -1;
	}

	return 0;
}

int yuyv_to_rgb(const unsigned char *yuv, char *file_name, int width, int height) // 640*480
{
    if (NULL == yuv || NULL == file_name){
        return -1;
    }
    if(0 == width || 0 == height) {
        return -1;
    }

    FILE *fp = fopen(file_name, "w");
    char *lineBuf = malloc(width * 3);
    int y, u, v;
    int r, g, b;
    for (int row = 0; row < height; ++row)
    {
        char *ptr = lineBuf;
        for (int col = 0; col < width; col += 2)
        {
            y = yuv[0];
            u = yuv[1];
            v = yuv[3];
            r = y + 1.402 * (v - 128);
            g = y - 0.344 * (u - 128) - 0.714 * (v - 128);
            b = y + 1.772 * (u - 128);
            ptr[0] = r > 255 ? 255 : (r < 0 ? 0 : r);
            ptr[1] = g > 255 ? 255 : (g < 0 ? 0 : g);
            ptr[2] = b > 255 ? 255 : (b < 0 ? 0 : b);

            y = yuv[2];
            u = yuv[1];
            v = yuv[3];
            r = y + 1.402 * (v - 128);
            g = y - 0.344 * (u - 128) - 0.714 * (v - 128);
            b = y + 1.772 * (u - 128);
            ptr[3] = r > 255 ? 255 : (r < 0 ? 0 : r);
            ptr[4] = g > 255 ? 255 : (g < 0 ? 0 : g);
            ptr[5] = b > 255 ? 255 : (b < 0 ? 0 : b);

            ptr += 6;
            yuv += 4;
        }
        fwrite(lineBuf, width * 3, 1, fp);
        fflush(fp);
    }
    return 0;
}

/*
 * @img: yuyv422格式图像原始数据
 * @fp: yuyv压缩为jpeg格式后的数据存放文件的文件描述符
 * @width: 图像宽度
 * @height: 图像高度
 * @quality: 压缩质量(1-100)
 */
int yuyv_to_jpeg(const unsigned char *yuv, char *file_name, int width, int height, int quality)
{
    FILE *fp = fopen(file_name, "w");
    struct jpeg_compress_struct cinfo; // 定义一个压缩对象.
    struct jpeg_error_mgr jerr;        // 用于存放错误信息.
    JSAMPROW row_pointer[1];           // 一行位图.

    cinfo.err = jpeg_std_error(&jerr); // 错误信息输出绑定到压缩对象.
    jpeg_create_compress(&cinfo);      // 初始化压缩对象.
    jpeg_stdio_dest(&cinfo, fp);       // 将保存输出数据的文件的文件描述符与压缩对象绑定.
    cinfo.image_width = width;
    cinfo.image_height = height;             // 图像的宽和高，单位为像素.
    cinfo.input_components = 3;              // 3表示彩色位图，如果是灰度图则为1.
    cinfo.in_color_space = JCS_RGB;          // JSC_RGB表示彩色图像.
    jpeg_set_defaults(&cinfo);               // 采用默认设置对图像进行压缩.
    jpeg_set_quality(&cinfo, quality, TRUE); // 设置图像压缩质量.
    jpeg_start_compress(&cinfo, TRUE);       // 开始压缩.

    // 申请buf空间，大小为yuyv数据转换为rgb格式后每一行的字节数.
    unsigned char *line_buf = calloc(width*3, 1); // 开辟行缓冲内存
    int r, g, b;
    int y, u, v; // 对每行yuv数据进行rgb转换
    while (cinfo.next_scanline < height)
    { // 逐行进行图像压缩
        unsigned char *ptr = line_buf;
		//对每行yuv数据进行rgb转换
        for (int col = 0; col < width; col += 2)
        {
            y = yuv[0];
            u = yuv[1];
            v = yuv[3];
            r = y + 1.402 * (v - 128);
            g = y - 0.344 * (u - 128) - 0.714 * (v - 128);
            b = y + 1.772 * (u - 128);
            ptr[0] = r > 255 ? 255 : (r < 0 ? 0 : r); //0x101
            ptr[1] = g > 255 ? 255 : (g < 0 ? 0 : g);
            ptr[2] = b > 255 ? 255 : (b < 0 ? 0 : b);

            y = yuv[2];
            u = yuv[1];
            v = yuv[3];
            r = y + 1.402 * (v - 128);
            g = y - 0.344 * (u - 128) - 0.714 * (v - 128);
            b = y + 1.772 * (u - 128);
            ptr[3] = r > 255 ? 255 : (r < 0 ? 0 : r);
            ptr[4] = g > 255 ? 255 : (g < 0 ? 0 : g);
            ptr[5] = b > 255 ? 255 : (b < 0 ? 0 : b);

            ptr += 6;
            yuv += 4;
        }
        row_pointer[0] = line_buf;
        jpeg_write_scanlines(&cinfo, row_pointer, 1); // 将行数据写入压缩对象
    }
    jpeg_finish_compress(&cinfo);  // 压缩完成
    jpeg_destroy_compress(&cinfo); // 释放申请的资源
    free(line_buf);
    return 0;
}

void stop_capture(int fd)
{
    global.capture = false;
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == ioctl(fd, VIDIOC_STREAMOFF, &type)) {
		perror("Fail to ioctl 'VIDIOC_STREAMOFF'");
		exit(EXIT_FAILURE);
	}

	return;
}

void uninit_camera()
{
	unsigned int i;

	for(i = 0; i < n_buffer; i ++) {
		if(-1 == munmap(camera_data[i].start, camera_data[i].length)) {
			exit(EXIT_FAILURE);
		}
	}

	free(camera_data);

	return;
}



