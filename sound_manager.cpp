
// 1. 加载音频资源并保存到map中
// 2. 建立线程并播放资源
// 3. 如果 接收到停止信号，或者 非循环状态下该音乐播放结束 则播放停止
// 4. 播放停止杀死线程

#include "sound_manager.h"
#include "link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <thread>

#define __STDC_CONSTANT_MACROS

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <SDL2/SDL.h>
}

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
 
#define MAX_AUDIO_SIZE 0x10000000 // 16mb

using namespace std;

struct soundObj
{
	uint8_t	*pcm_buffer;
	int		pcm_buffer_size;
};

struct soundPlayingObj
{
	//Buffer:
	//|-----------|-------------|
	//chunk-------pos---len-----|
	// static  Uint8  *audio_chunk; 
	// static  Uint32  audio_len; 
	// static  Uint8  *audio_pos; 

	Uint8	*audio_chunk; 
	Uint32	audio_len; 
	Uint8	*audio_pos;

	bool 	isPlayEnd;
	bool	isReadEnd;
};

static map<string, soundObj> soundList;
static Link<soundPlayingObj> soundPlayingList;

// 0. 数据读取
// 2. 过滤数据
// 3. 开始delay
enum audio_status
{
	AUDIO_STATUS_READ,
	AUDIO_STATUS_DELAY
};

static audio_status status = AUDIO_STATUS_READ;

void fill_audio(void *udata, Uint8 *stream, int len)
{
    Link<soundPlayingObj>* sound_playing_list = (Link<soundPlayingObj>*)udata;
	//SDL 2.0
	SDL_memset(stream, 0, len);

    sound_playing_list->exec_erase();

    Node<soundPlayingObj> *next, *node;
    node = sound_playing_list->head;
    for(;;)
    {
        if (node == nullptr) break;
        next = node->next;

        if (node->node->audio_len <= 0)
        {
            node = next;
            continue;
        }

        int new_len = (len > node->node -> audio_len ? node->node -> audio_len : len);	/*  Mix  as  much  data  as  possible  */

        SDL_MixAudio(stream, node->node -> audio_pos, new_len, SDL_MIX_MAXVOLUME);

        node->node->audio_pos += new_len;
        node->node->audio_len -= new_len;

        node = next;
    }
}

/* The audio function callback takes the following parameters: 
 * stream: A pointer to the audio buffer to be filled 
 * len: The length (in bytes) of the audio buffer 
*/ 
//-----------------

sound_manager::sound_manager()
{
}

sound_manager::~sound_manager()
{
}

// sdl_audio thread
void* create_init_th(void* args)
{
	av_register_all();
	avformat_network_init();

	//SDL_AudioSpec
	SDL_AudioSpec wanted_spec;
	wanted_spec.freq = 44100; 
	wanted_spec.format = AUDIO_S16SYS; 
	wanted_spec.channels = 2; 
	wanted_spec.silence = 0; 
	wanted_spec.samples = 1024;
	wanted_spec.callback = fill_audio;
	wanted_spec.userdata = (void*) &soundPlayingList;

	//Init
	if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("Could not initialize SDL - %s\n", SDL_GetError()); 
	}

	if (SDL_OpenAudio(&wanted_spec, NULL) < 0)
	{ 
		printf("can't open audio.\n");
	}

	//Play
	SDL_PauseAudio(0);
	// 在播放前过滤元素
    Node<soundPlayingObj> *next, *node;
	for (;;)
	{
		if (status == AUDIO_STATUS_READ)
		{
		    if (soundPlayingList.empty()) continue;

            node = soundPlayingList.head;
            for(;;)
            {
                if (node == nullptr) break;
                next = node->next;

                if(!node->node->isReadEnd) break;

                node = next;
            }
			if (node == nullptr)
			{
				status = AUDIO_STATUS_DELAY;
			}
		}

		if (status == AUDIO_STATUS_DELAY)
		{
            for (;;)
            {
                if (soundPlayingList.empty())
                {
                    status = AUDIO_STATUS_READ;
                    continue;
                }

                node = soundPlayingList.head;
                for(;;)
                {
                    if (node == nullptr) break;
                    next = node->next;

                    if(node->node->audio_len > 0) break;

                    node = next;
                }

                if (node == nullptr) break;

                SDL_Delay(1);
            }

            node = soundPlayingList.head;
            for(;;)
            {
                if (node == nullptr) break;
                next = node->next;

                node->node->isReadEnd = false;

                node = next;
            }

			status = AUDIO_STATUS_READ;
		}
	}
}


// play new audio thread
void* create_play_th(void* args)
{
	playObj* po = (playObj*)args;
    soundObj* sound = &soundList[po->url];

	int frame_buffer_size = 4096;
	char *frame_buffer = (char *)malloc(frame_buffer_size);
	int data_count = 0;
 
	soundPlayingObj* spo;
    spo = new soundPlayingObj;
	spo->isPlayEnd = false;
	spo->isReadEnd = false;

	bool is_init = false;

	for(;;)
	{
		if (status == AUDIO_STATUS_READ && !spo->isReadEnd)
		{
			if (sound -> pcm_buffer_size - data_count <= 0)
			{
				if (!po->isLoop)
				{
					spo->isReadEnd = true;
                    spo->isPlayEnd = true;
                    soundPlayingList.pre_erase(spo);
					break;
				}
				// Loop
				data_count = 0;
			}

			for (int j = 0; j < frame_buffer_size; ++j)
			{
				*(frame_buffer + j) = *(sound -> pcm_buffer + data_count + j);
			}

			// printf("Now Playing %10d Bytes data.\n",data_count);
			data_count += frame_buffer_size;
			//Set audio buffer (PCM data)
            spo->audio_chunk = (Uint8 *)frame_buffer;
			//Audio buffer length
			spo->audio_len = frame_buffer_size;
			spo->audio_pos = spo->audio_chunk;

            spo->isReadEnd = true;
            if (!is_init)
            {
                soundPlayingList.push(spo);
                is_init = true;
            }
		}
	}


	free(frame_buffer);
	delete po;
}

void sound_manager::init()
{
	pthread_t th;
    pthread_create(&th, NULL, create_init_th, NULL);
}

int sound_manager::load(string url)
{
	soundObj sound;

	// wav不用解码
	if (url.find(".wav") != string::npos)
	{
		FILE *fp = fopen(url.c_str(), "rb+");
		if (fp == NULL)
		{
			printf("cannot open this file\n");
			return -1;
		}

		sound.pcm_buffer = (uint8_t *)av_malloc(MAX_AUDIO_SIZE * 2);

		int pcm_buffer_size = 4096;
		char *pcm_buffer = (char *)malloc(pcm_buffer_size);
		int data_count = 0;

		for(;;)
		{
			int frame_size = fread(pcm_buffer, 1, pcm_buffer_size, fp);

			if (frame_size != pcm_buffer_size)
			{
				break;
			}

			for (int j = 0; j < pcm_buffer_size; ++j)
			{
				*(sound.pcm_buffer + data_count + j) = *(pcm_buffer + j);
			}

			data_count += frame_size;
		}

		sound.pcm_buffer_size = data_count;
    	soundList[url] = sound;

		free(pcm_buffer);
		fclose(fp);

		return 0;
	}

	AVFormatContext	*pFormatCtx;
	int				i, audioStream;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVPacket		*packet;
	uint8_t			*out_buffer;
	AVFrame			*pFrame;
    int ret;
	uint32_t len = 0;
	int got_picture;
	int index = 0;
	int64_t in_channel_layout;
	struct SwrContext *au_convert_ctx;

	// av_register_all();
	// avformat_network_init();
	pFormatCtx = avformat_alloc_context();
	//Open
	if(avformat_open_input(&pFormatCtx, url.c_str(), NULL, NULL) != 0)
	{
		printf("Couldn't open input stream.\n");
		return -1;
	}
	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx,NULL) < 0)
	{
		printf("Couldn't find stream information.\n");
		return -1;
	}
	// Dump valid information onto standard error
	av_dump_format(pFormatCtx, 0, url.c_str(), false);
 
	// Find the first audio stream
	audioStream = -1;
	for (i = 0; i < pFormatCtx -> nb_streams; i++)
		if(pFormatCtx -> streams[i] -> codec -> codec_type == AVMEDIA_TYPE_AUDIO){
			audioStream = i;
			break;
		}
 
	if (audioStream == -1)
	{
		printf("Didn't find a audio stream.\n");
		return -1;
	}
 
	// Get a pointer to the codec context for the audio stream
	pCodecCtx = pFormatCtx -> streams[audioStream] -> codec;

	// Find the decoder for the audio stream
	pCodec = avcodec_find_decoder(pCodecCtx -> codec_id);
	if (pCodec == NULL)
	{
		printf("Codec not found.\n");
		return -1;
	}
 
	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
	{
		printf("Could not open codec.\n");
		return -1;
	}
 
	packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	av_init_packet(packet);

	//Out Audio Param
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	//nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = pCodecCtx -> frame_size;
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	int out_sample_rate = 44100;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	//Out Buffer Size
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
 
	out_buffer = (uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
	pFrame = av_frame_alloc();

	//FIX:Some Codec's Context Information is missing
	in_channel_layout = av_get_default_channel_layout(pCodecCtx -> channels);
	//Swr
	au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,out_channel_layout, out_sample_fmt, out_sample_rate,
		in_channel_layout, pCodecCtx -> sample_fmt, pCodecCtx -> sample_rate, 0, NULL);
	swr_init(au_convert_ctx);

	int tmp_size = 0;
	sound.pcm_buffer = (uint8_t *)av_malloc(MAX_AUDIO_SIZE * 2);

	while (av_read_frame(pFormatCtx, packet) >= 0)
	{
		if (packet -> stream_index == audioStream)
		{
			ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0)
			{
                printf("Error in decoding audio frame.\n");
                return -1;
            }
			if (got_picture > 0)
			{
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE,(const uint8_t **)pFrame -> data , pFrame -> nb_samples);
 
				// printf("index:%5d\t pts:%lld\t packet size:%d\n",index,packet -> pts,packet -> size);

				for (int j = 0; j < out_buffer_size; ++j)
				{
					*(sound.pcm_buffer + tmp_size + j) = *(out_buffer + j);
				}

				tmp_size += out_buffer_size;

				index++;
			}
		}
		av_free_packet(packet);
	}

	sound.pcm_buffer_size = tmp_size;
    soundList[url] = sound;

	swr_free(&au_convert_ctx);

	av_free(out_buffer);
	// Close the codec
	avcodec_close(pCodecCtx);
	// Close the video file
	avformat_close_input(&pFormatCtx);
 
	return 0;
}

int sound_manager::play(playObj* po)
{
	pthread_t th;
	int ret = pthread_create(&th, NULL, create_play_th, (void*)po);
	return ret;
}

void sound_manager::close()
{
	SDL_Quit();
}
