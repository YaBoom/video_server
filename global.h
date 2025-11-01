#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <stdbool.h>
#include <pthread.h>

typedef struct {
    bool capture;	//0表示不采集视频数据，1表示采集视频数据.
	void *start; //存放视频数据的起始地址.
	int  length; //视频数据的长度.
	pthread_mutex_t update_lock; //线程锁.
	pthread_cond_t  update_cond; //线程条件变量.
} global_t;

extern global_t global;



#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define LENGTH_OF(x) (sizeof(x)/sizeof(x[0]))

#define PICTURE_SIZE (1024 * 1024 * 1024) //预分配图片的大小:1GB.

#define BUFFER_SIZE 1024

#endif //_GLOBAL_H_