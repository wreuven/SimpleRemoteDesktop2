
#include "config.h"
#include "network.h"

#include <stdio.h> 
#include <string.h> 
#include <unistd.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define _write write
#define _close close


#define FIFO 1

void init_video_fifo()
{
	static Video_Buffer _video_fifo;
	video_fifo = &_video_fifo;
	video_fifo->first = NULL;
	video_fifo->length = 0;
}

int init_network()
{

	init_video_fifo();
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Init network interface");
	if(SDLNet_Init() < 0 ) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDLNet_Init: %s\n", SDLNet_GetError());
		return 0;
	}

	if(SDLNet_ResolveHost(&ip, configuration->server->hostname, configuration->server->port) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "unable to resolve address %s , port %d\n", 
				configuration->server->hostname,
			       	configuration->server->port);
		return 0;
	} 
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "connecting to server");
	if(!(control_socket = SDLNet_TCP_Open(&ip))) {
		 SDL_LogError(SDL_LOG_CATEGORY_ERROR,"SDLNet_TCP_Open: %s\n", SDLNet_GetError());
		return 0;
	}
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "connected to server");
	
	netThread = SDL_CreateThread(network_thread, "network_thread", configuration);
	return 0;
}

int SRDNet_send_start_packet() 
{
	int len;

	// inital packet with information
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, " network : sending start packet");
	struct Message init;
	init.type = TYPE_ENCODER_START;
	init.x = 1;
	init.y = 1;
	init.button = 1;
	init.keycode = 1;
	init.fps = configuration->fps;
	init.codec_width = configuration->codec->width;
	init.codec_height = configuration->codec->height;
	init.bandwidth = configuration->bandwidth;
	init.sdl = 0;
	SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "sending init frame : type: %d, fps: %d, codec width: %d, codec height: %d, bandwidth: %d", init.type, init.fps, init.codec_width, init.codec_height, init.bandwidth);
	len = SDLNet_TCP_Send(control_socket, (void * )&init, sizeof(init));
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "network : sent start packet (%d bytes)", len);
	return 0;

}

int SRDNet_send_stop_packet() 
{
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, " network : sending stop packet");
	struct Message stop;
	stop.type = TYPE_ENCODER_STOP;
	SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "sending stop frame"); 
	SDLNet_TCP_Send(control_socket, (void * )&stop, sizeof(stop));
	return 0;
}

int SRDNet_get_frame_number()
{
	return SRD_readUInt32();
}

int SRDNet_get_frame_length()
{
	return SRD_readUInt32();
}


void SRDNet_Empty_input_buffer()
{
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "cleaning input tcp buffer");
	char net_in[2048];
	int i;

	do {
		i = SDLNet_TCP_Recv(control_socket, net_in, 1024);
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%d bytes remains", i);
	}
	while( i > 0 );
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "cleaned");

}

int SRD_readUInt32()
{
	uint8_t *data = SRD_read(4);
	SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, " %x %x %x %x \n", data[0], data[1], data[2], data[3]);
	uint32_t num = 
		(uint32_t) data[0] << 24 |
		(uint32_t) data[1] << 16 |
		(uint32_t) data[2] << 8  |
		(uint32_t) data[3];

	return num;
}

#define MAX_FRAME_SIZE 4*1024*1024

static uint8_t data[MAX_FRAME_SIZE];

uint8_t *SRD_read(int nbytes)
{
	int len;
	uint8_t* ptr = data;

	while (nbytes > 0) {
		len = SDLNet_TCP_Recv(control_socket, ptr, nbytes);
		nbytes -= len;
		ptr += len;
	}
	return data;
}

int SRD_read2(uint8_t *ptr, int nbytes)
{
  return (SDLNet_TCP_Recv(control_socket, ptr, nbytes));
}

void video_terminate();
int  video_init();
void video_feed();

int network_thread(void* configuration) 
{
	static Video_Frame _frame;
	Video_Frame *frame = &_frame;

	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "starting network thread");

	video_init();
	video_feed();

	while(true) 
	{
		
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "network: reading frame");

		// get frame from network
		frame->number =  SRDNet_get_frame_number();
		frame->length =  SRDNet_get_frame_length();
		frame->data   =  SRD_read(frame->length);

		printf("%d:%d\n", frame->number, frame->length);

		static int fd = -1;
		static int len = 0;
		int ret;

		static char* filename = "/tmp/out.h264";
		if (len >= 0) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "initializing fifo");
			if (fd < 0) {
				int retval = remove(filename);
				mkfifo(filename, 0666);
				fd = open(filename, O_WRONLY, 0666);
			}
			ret = _write(fd, frame->data, frame->length);
			len += frame->length;
		}
	}
}



