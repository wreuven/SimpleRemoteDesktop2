// remote desktop sdl client
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>

// FIXME remove all NULL => must be restore

Uint8 *yPlane, *uPlane, *vPlane;
size_t yPlaneSz, uvPlaneSz;
int uvPitch;

int init_video_decoder(int codec_width, int codec_height);
int decode_video_frame(uint8_t *frame,int frame_length, Configuration *conf); 
void destroy_decoder();
