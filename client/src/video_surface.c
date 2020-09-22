#include "config.h"
#include "client.h"
#include "video_surface.h"
#include "video_decoder.h"
#include "network.h"

void init_video(int screen_width, int screen_height)
{
	// Make a screen to put our video
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, " Creating SDL windows size %dx%d", screen_width, screen_height);
	screen = SDL_CreateWindow(
			"StreamMyDesktop Client",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			screen_width,
			screen_height,
			0); 

}

void SRD_init_renderer_texture(int w, int h)
{
}

void destroy_texture()
{
}

void update_video_surface() 
{

}
void SRD_UpdateScreenResolution() 
{
}
