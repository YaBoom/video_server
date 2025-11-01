#ifndef _SERVER_H_
#define _SERVER_H_

#include <pthread.h>

/*
 * Standard header to be send along with other header information like mimetype.
 * The parameters should ensure the browser does not cache our answer.
 * A browser should connect for each file and not serve files from his cache.
 * Using cached pictures would lead to showing old/outdated pictures
 * Many browser seem to ignore, or at least not always obey those headers
 * since i observed caching of files from time to time.
 */
#define STD_HEADER "Connection: close\r\n" \
                   "Server: MJPG-Streamer/0.2\r\n" \
                   "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n" \
                   "Pragma: no-cache\r\n" \
                   "Expires: Mon, 3 Jan 2013 12:34:56 GMT\r\n"


/* the boundary is used for the M-JPEG stream, it separates the multipart stream of pictures */
#define BOUNDARY "cyg-boundary"
#define WEB_DIR "www"

/* the webserver determines between these values for an answer */
typedef enum {
	A_UNKNOWN, 
	A_SNAPSHOT, 
	A_STREAM, 
	A_COMMAND, 
	A_FILE
} answer_t;

//HTTP 请求
typedef struct {
	answer_t type;
	char *parm;
} request_t;

extern int init_tcp(const char *ipstr, unsigned short port, int backlog);
extern void *client_thread(void *arg);
extern  int analyse_http_request(const char *buf, request_t *req);
extern void send_file(int sockfd, char *pathfile);
extern void send_snapshot(int sockfd);
extern void send_stream(int sockfd);



#endif //_SERVER_H_
