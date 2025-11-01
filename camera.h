#ifndef _CAMERA_H_
#define _CAMERA_H_

#define IMG_WIDTH  640
#define IMG_HEIGHT 480
#define IMG_QUALITY 90

struct camera_t{
    void *start;
    int length;
};

extern int init_camera(const char *dev_path);
extern  int init_mmap(int fd);
extern void *start_capturing(void *arg);
extern  int read_frame(int fd);
extern void stop_capture(int fd);
extern void uninit_camera();

extern int yuyv_to_rgb(const unsigned char *yuv, char *file_name, int width, int height);
extern int yuyv_to_jpeg(const unsigned char *yuv, char *file_name, int width, int height, int quality);

#endif //_CAMERA_H_