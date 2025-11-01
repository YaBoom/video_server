
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>

#include "global.h"
#include "camera.h"
#include "server.h"

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

global_t global;

void signal_handler(int signum)
{
	/*
	 *关闭设备
	 */
    //global.
    return;
}

void init_global(global_t *pglobal)
{
    pglobal->capture = true;
    pglobal->length = 0;

	if((global.start = malloc(PICTURE_SIZE)) == NULL) {
		perror("Fail to malloc");
		exit(EXIT_FAILURE);
	}

	if(pthread_mutex_init(&(pglobal->update_lock), NULL) < 0) {
		perror("Fail to pthread_mutex_init");
		exit(EXIT_FAILURE);
	}

	if(pthread_cond_init(&(pglobal->update_cond), NULL) < 0) {
		perror("Fail to pthread_cond_init");
		exit(EXIT_FAILURE);
	}

	return;
}

int main()
{
	//忽略SIGPIPE信号.
	if(signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		perror("Fail to signal");
		exit(EXIT_FAILURE);
	}

	//捕捉ctrl+c信号.
	// if(signal(SIGINT, signal_handler) == SIG_ERR) {
	// 	perror("Fail to signal");
	// 	exit(EXIT_FAILURE);
	// }
	
	//初始化全局变量global.
	init_global(&global);

	//初始化视频设备.
    int cam_fd = init_camera("/dev/video0");
    if(-1 == cam_fd){
        perror("init_camera() failure!");
        return -1;
    }

	//内存映射
    if(-1 == init_mmap(cam_fd)){
        perror("init_mmap() failure!");
        return -1;
    }

	//开始捕获图像.
	pthread_t cam_tid;
	int ret = pthread_create(&cam_tid, NULL, start_capturing, (void *)cam_fd);
	if(ret != 0) {
		perror("Fail to pthread_create");
		exit(EXIT_FAILURE);
	}

    int s_fd = init_tcp("127.0.0.1", 8080, 5);

    while (1) { /*并发接收客户端发来的请求*/
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int rws = accept(s_fd, (struct sockaddr *)&addr, &len);
        if (0 > rws) {
            perror("Fail to accept");
            continue;
        }
        pthread_t tid;
        int ret = pthread_create(&tid, NULL, client_thread, (void *)rws);
        if (ret != 0) {
            perror("Fail to pthread_create");
            continue;
        }

        pthread_detach(tid);
        printf("loop again\n");
    }

    stop_capture(cam_fd);

    uninit_camera(cam_fd);

    close(cam_fd);

    return 0;
}
