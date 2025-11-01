mjpg_streamer:main.o server.o camera.o
	gcc main.o server.o camera.o -o mjpg_streamer -lpthread -ljpeg

main.o:main.c
	gcc -c -g main.c -o main.o

server.o:server.c
	gcc -c -g server.c -o server.o

camera.o:camera.c
	gcc -c -g camera.c -o camera.o


clean:
	rm -rf *.o mjpg_streamer
