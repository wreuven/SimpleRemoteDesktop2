/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Video deocode demo using OpenMAX IL though the ilcient helper library

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcm_host.h"
#include "ilclient.h"

#define MIN(x,y) (((x) < (y)) ? (x) : (y))

OMX_VIDEO_PARAM_PORTFORMATTYPE format;
COMPONENT_T* video_decode = NULL, * video_render = NULL;
COMPONENT_T* list[2];
TUNNEL_T tunnel[1];
ILCLIENT_T* client;
int status = 0;
unsigned int data_len = 0;
int port_settings_changed = 0;
int first_packet = 1;
OMX_BUFFERHEADERTYPE* buf;

void video_terminate() {

    buf->nFilledLen = 0;
    buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;

    // deliver this buffer with an EOS to the decoder
    if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone)
        status = -20;
 
    // wait for EOS from renderer
    ilclient_wait_for_event(video_render, OMX_EventBufferFlag, 90, 0, OMX_BUFFERFLAG_EOS, 0,
        ILCLIENT_BUFFER_FLAG_EOS, -1);

    // need to flush the renderer to allow video_decode to disable its input port
    ilclient_flush_tunnels(tunnel, 0);

    ilclient_disable_tunnel(tunnel);
    ilclient_disable_tunnel(tunnel + 1);
    ilclient_disable_tunnel(tunnel + 2);
    ilclient_disable_port_buffers(video_decode, 130, NULL, NULL, NULL);
    ilclient_teardown_tunnels(tunnel);

    ilclient_state_transition(list, OMX_StateIdle);
    ilclient_state_transition(list, OMX_StateLoaded);

    ilclient_cleanup_components(list);

    OMX_Deinit();

    ilclient_destroy(client);
}

int SRDNet_get_frame_number();
int SRDNet_get_frame_length();
int SRD_read2(uint8_t *ptr, int nbytes);

int video_feed() {

    int frame_len = 0;

    while ((buf = ilclient_get_input_buffer(video_decode, 130, 1)) != NULL)
    {
        // feed data and wait until we get port settings changed
        unsigned char* dest = buf->pBuffer;

	if (frame_len == 0) {
		int number	=  SRDNet_get_frame_number();
		frame_len  	=  SRDNet_get_frame_length();
		printf("%d:%d\n", number, frame_len);
	}

	int bytes_to_read = MIN(buf->nAllocLen, frame_len); 

        int bytes_read    = SRD_read2(dest, bytes_to_read);

	frame_len -= bytes_read;

        // complete the creation of the play graph after video decoder knows what it is decoding

        if (port_settings_changed == 0 &&
            ((data_len > 0 && ilclient_remove_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1) == 0) ||
                (data_len == 0 && ilclient_wait_for_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1,

                    ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 1) == 0)))
        {
            port_settings_changed = 1;

            // now setup tunnel to video_render
            if (ilclient_setup_tunnel(tunnel, 0, 0) != 0)
            {
                status = -12;
                break;
            }

            ilclient_change_component_state(video_render, OMX_StateExecuting);
        }

        if (!bytes_read)
            break;

        buf->nFilledLen = bytes_read;
        buf->nOffset = 0;

        if (first_packet)
        {
            buf->nFlags = OMX_BUFFERFLAG_STARTTIME;
            first_packet = 0;
        }
        else
            buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;

        if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone)
        {
            status = -6;
            break;
        }

    }

    return 0;
}

int video_init(char* filename)
{
    bcm_host_init();

    memset(list, 0, sizeof(list));
    memset(tunnel, 0, sizeof(tunnel));

    if ((client = ilclient_init()) == NULL)
    {
        return -3;
    }

    if (OMX_Init() != OMX_ErrorNone)
    {
        ilclient_destroy(client);
        return -4;
    }

    // create video_decode
    if (ilclient_create_component(client, &video_decode, "video_decode", ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS) != 0)
        status = -14;
    list[0] = video_decode;

    // create video_render
    if (status == 0 && ilclient_create_component(client, &video_render, "video_render", ILCLIENT_DISABLE_ALL_PORTS) != 0)
        status = -14;
    list[1] = video_render;

    set_tunnel(tunnel, video_decode, 131,  video_render, 90);
	
    // I copied this config from somehwere. I don't think it is doing anything.

    OMX_CONFIG_LATENCYTARGETTYPE latencyTarget;
    memset(&latencyTarget, 0, sizeof(OMX_CONFIG_LATENCYTARGETTYPE));
    latencyTarget.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    latencyTarget.nVersion.nVersion = OMX_VERSION;
    latencyTarget.nPortIndex = 90;
    latencyTarget.bEnabled = OMX_TRUE;
    latencyTarget.nFilter = 1;
    latencyTarget.nTarget = 0;
    latencyTarget.nShift = 7;
    latencyTarget.nSpeedFactor = 512;
    latencyTarget.nInterFactor = 500;
    latencyTarget.nAdjCap = 20;

    if(OMX_SetParameter(ILC_GET_HANDLE(video_render), OMX_IndexConfigLatencyTarget, &latencyTarget) != OMX_ErrorNone) {
        fprintf(stderr, "Failed to set video render latency parameters\n");
        exit(EXIT_FAILURE);
    }


   {
           // I copied this config from somehwere. I was expecting full screen but not working.

	   OMX_CONFIG_DISPLAYREGIONTYPE region;
	   memset(&region, 0, sizeof(OMX_CONFIG_DISPLAYREGIONTYPE));
	   region.nSize = sizeof(OMX_CONFIG_DISPLAYREGIONTYPE);
	   region.nVersion.nVersion = OMX_VERSION;
	   region.nPortIndex = 90;
	   region.layer = 2;
	   region.set |= OMX_DISPLAY_SET_LAYER;
    	   region.fullscreen = OMX_TRUE;
    	   region.mode = OMX_DISPLAY_SET_FULLSCREEN;
	   if (OMX_SetParameter(ILC_GET_HANDLE(video_render), OMX_IndexConfigDisplayRegion, &region) != OMX_ErrorNone) {
	        fprintf(stderr, "Failed to set video render region parameters\n");
	        exit(EXIT_FAILURE);
	   }
		
   }

    if (status == 0)
        ilclient_change_component_state(video_decode, OMX_StateIdle);

    memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
    format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
    format.nVersion.nVersion = OMX_VERSION;
    format.nPortIndex = 130;
    format.eCompressionFormat = OMX_VIDEO_CodingAVC;

    if (status == 0 &&
        OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamVideoPortFormat, &format) == OMX_ErrorNone &&
        ilclient_enable_port_buffers(video_decode, 130, NULL, NULL, NULL) == 0)
    {

        ilclient_change_component_state(video_decode, OMX_StateExecuting);
        video_feed();
        video_terminate();
    }

    return 0;
 }

