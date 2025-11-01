#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "global.h"
#include "server.h"

/*
 * Only the following fileypes are supported.
 *
 * Other filetypes are simply ignored!
 * This table is a 1:1 mapping of files extension to a certain mimetype.
 */
static const struct {
	const char *dot_extension;
	const char *mimetype;
} mimetypes[] = {
	{ ".html", "text/html" }, 
	{ ".htm",  "text/html" }, 
	{ ".css",  "text/css" }, 
	{ ".js",   "text/javascript" }, 
	{ ".txt",  "text/plain" }, 
	{ ".jpg",  "image/jpeg" }, 
	{ ".jpeg", "image/jpeg" }, 
	{ ".png",  "image/png"}, 
	{ ".gif",  "image/gif" }, 
	{ ".ico",  "image/x-icon" }, 
	{ ".swf",  "application/x-shockwave-flash" }, 
	{ ".cab",  "application/x-shockwave-flash" }, 
	{ ".jar",  "application/java-archive" }
};

int init_tcp(const char *ip_str, unsigned short port, int backlog)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Fail to socket");
		exit(EXIT_FAILURE);
    }

    int on = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		perror("Fail to setsockopt");
		exit(EXIT_FAILURE);
	}

    printf("ip: %#X\n", inet_aton(ip_str, NULL));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = inet_addr(ipstr),
        .sin_zero = {0},
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Fail to bind");
		exit(EXIT_SUCCESS);
    }

    listen(fd, backlog);
    printf("Listen fd %d port %d\n", fd, port);
    printf("server ip is %s\n", inet_ntoa(addr.sin_addr));

    return fd;
}


void *client_thread(void *arg)
{
    int rws = (int)arg;
    char buf[1024] = {0};

    if (0 > recv(rws, buf, sizeof(buf) - 1, 0))
    {
        perror("recv() failure");
        goto end;
    }

    request_t request = {
        .type = 0,
        .parm = NULL, //这里需要开辟空间存放http数据.
    };

    if (analyse_http_request(buf, &request) < 0){
        perror("analyse_http_request() failure");
        goto end;
    }

    switch (request.type){
        case A_FILE:
        send_file(rws, request.parm);
        break;
    case A_SNAPSHOT:
        send_snapshot(rws);
        break;
    case A_STREAM:
        send_stream(rws);
        break;
    }

end:
    close(rws);

    if(NULL != request.parm){
        free(request.parm);
    }

    return NULL;
}

int analyse_http_request(const char *buf, request_t *req)
{
    if (NULL != strstr(buf, "GET /?action=snapshot"))
    {
        req->type = A_SNAPSHOT;
		return 0;
    }
    else if (NULL != strstr(buf, "GET /?action=stream"))
    {
        req->type = A_STREAM;
		return 0;
    }
    else {
        char *url = strstr(buf, "GET /");
        if (NULL == url)
        {
            printf("Http request error!\n");
			//send error to client
			return -1;
        }
        url += strlen("GET /");
        char arr[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._-1234567890";
        int len = MIN(MAX(strspn(url, arr), 0), 100);
        req->parm = (char *)malloc(len + 1);
        memset(req->parm, 0, len + 1);
        memcpy(req->parm, url, len);
        printf("req->parm : %s.\n", req->parm);
        req->type = A_FILE;
        return 0;
    }
}

void send_snapshot(int sockfd)
{
    int length;
    char *frame;
	char buf[BUFFER_SIZE];

    printf("send_snapshot(%d)\n", sockfd);
    pthread_mutex_lock(&global.update_lock);

	pthread_cond_wait(&global.update_cond, &global.update_lock);

	//获得视频数据
	length = global.length;
	frame  = (char *)malloc(global.length);
	memcpy(frame, global.start, global.length);

	pthread_mutex_unlock(&global.update_lock);

	//添加http头
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "HTTP/1.0 200 OK\r\n" \
	        "Content-type: image/jpeg\r\n" \
	        STD_HEADER \
	        "\r\n");

	//发送http头
	if(0 > send(sockfd, buf, strlen(buf), 0)) {
        printf("send_snapshot() send http head failure\n");
        free(frame);
		return;
	}

    //发送视频数据
    if(0 > send(sockfd, frame, length, 0)){
        printf("send_snapshot() send frame failure\n");
    }

    free(frame);
    return;
}

void send_stream(int sockfd)
{
	int length;
	char buf[BUFFER_SIZE];

	printf("send_stream(%d)\n", sockfd);

	sprintf(buf, "HTTP/1.1 200 OK\r\n" \
	        STD_HEADER \
	        "Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n" \
	        "\r\n" \
	        "--" BOUNDARY "\r\n");

	if(send(sockfd, buf, strlen(buf), 0) < 0) {
        perror("send_stream() fail to send http head");
        return;
	}

    char *frame = NULL;
    while (global.capture) //how to stop?
    {
        pthread_mutex_lock(&global.update_lock);
		pthread_cond_wait(&global.update_cond, &global.update_lock);

		length = global.length;
		frame  = (char *)malloc(global.length);
		memcpy(frame, global.start, global.length);

		pthread_mutex_unlock(&global.update_lock);

		/* print the individual mimetype and the length
		 * sending the content-length fixes random stream disruption observed
		 * with firefox
		 */
		sprintf(buf, "Content-Type: image/jpeg\r\n" \
		        "Content-Length: %d\r\n" \
		        "\r\n", length);
		
		if(0 > send(sockfd, buf, strlen(buf), 0)) {
            perror("send_stream() Fail to send http header");
            break;
		}

		if(0 > send(sockfd, frame, length, 0)) {
            perror("send_stream() Fail to send camera frame");
            break;
		}

		sprintf(buf, "\r\n--" BOUNDARY "\r\n");
		if (0 > send(sockfd, buf, strlen(buf), 0)) {
            break;
        }
        free(frame);
        usleep(100);
    }

    free(frame);
    return;
}

void send_file(int sockfd, char *pathfile)
{
	int n, fd;
	char buf[1024] = {0};
	char *extension, *mimetype=NULL;

    if (pathfile == NULL || strlen(pathfile) == 0)
        pathfile = "index.html";

    /* find file-extension */
    if (NULL == (extension = strstr(pathfile, ".")))
    {
        //    send_error(fd, 400, "No file extension found");
        return;
    }

    /* determine mime-type */
    for (int i = 0; i < sizeof(mimetypes) / sizeof(mimetypes[0]); ++i)
    {
		if (0 == strcmp(mimetypes[i].dot_extension, extension)) {
			mimetype = (char *)mimetypes[i].mimetype;
			break;
		}
	}

    if (NULL == mimetype)
    {
        return;
    }

	//打开文件
	sprintf(buf, "%s/%s", WEB_DIR, pathfile);
	if((fd = open(buf, O_RDONLY)) < 0) {
		fprintf(stderr, "Fail to open %s : %s.\n", buf, strerror(errno));
		//send error to webbrowser
		return;
	}

    // 添加http头.
    memset(buf, 0, sizeof(buf));
    sprintf(buf, "HTTP/1.0 200 OK\r\n" \
                 "Content-type: %s\r\n" \
                 STD_HEADER \
	        "\r\n", mimetype); //注意这里会多出个空行.

    // 发送http头信息和网页文件数据.
	n = strlen(buf);

	//发送http头信息和网页文件数据.
	do {
		if(0 > send(sockfd, buf, n, 0)){
            perror("send_file() Fail to send file contain");
        }
    } while (n = read(fd, buf, sizeof(buf)));

    return;
}

