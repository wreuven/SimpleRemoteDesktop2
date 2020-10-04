#include <SDL2/SDL_net.h>

#include <stdio.h> 
#include <string.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <errno.h>
#include <unistd.h> 

#include "config.h"
#include "video_decoder.h"
#include "video_surface.h"


int init_video_decoder(int codec_width, int codec_height)
{
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "starting with codec resolution %dx%d", codec_width, codec_height);
	



    return 1; // TODO
}

void destroy_decoder()
{
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, " Closing decoder");
}

int decode_video_frame(uint8_t *frame,int frame_length, Configuration *conf) 
{

  return 1; //TODO
}


void free_video_decoder() //FIXME need to clean ?
{

}


