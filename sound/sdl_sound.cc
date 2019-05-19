/*
 * sdl-sound without SDL_mixer.dll
 *
 * This file is part of the Simutrans project under the artistic license.
 */


#include "SDL.h"
#include <string.h>
#include "sound.h"
#include "../simmem.h"
#include "../simdebug.h"

/*
 * Hajo: flag if sound module should be used
 */
static int use_sound = 0;

/*
 * defines the number of channels avaiable
 */
#define CHANNELS 4


/*
 * this structure contains the data for one sample
 */
struct sample {
    /* the buffer containing the data for the sample, the format
     * must always be identical to the one of the system output
     * format */
    Uint8 *audio_data;

    Uint32 audio_len;		    /* number of samples in the adiop data */
};


/* this list contains all the samples
 */
static sample samples[1024];

/* all samples are stored chronologically there
 */
static int samplenumber = 0;


/* this structure contains the information about one channel
 */
struct channel {
    Uint32 sample_pos; /* the current position inside this sample */
    Uint8 sample;		/* which sample is played, 255 for no sample */
    Uint8 volume;		/* the volume this channel should be played */
};


/* this array contains all the information of the currently played samples
 */
static channel channels[CHANNELS];


/* the format of the output audio channel in use
 * all loaded waved need to be converted to this format
 */
static SDL_AudioSpec output_audio_format;


void sdl_sound_callback(void *, Uint8 * stream, int len)
{
	memset(stream, 0, len); // SDL2 requires the output stream to be fully written on every callback.

	/*
	* add all the sample that need to be played
	*/
	for(  int c = 0;  c < CHANNELS;  c++  ) {
		/*
		* only do something, if the channel is used
		*/
		if (channels[c].sample != 255) {
			sample * smp = &samples[channels[c].sample];

			/*
			* add sample
			*/
			if (len + channels[c].sample_pos >= smp->audio_len ) {
				// SDL_MixAudio(stream, smp->audio_data + channels[c].sample_pos, smp->audio_len - channels[c].sample_pos, channels[c].volume);
				channels[c].sample = 255;
			}
			else {
				SDL_MixAudio(stream, smp->audio_data + channels[c].sample_pos, len, channels[c].volume);
				channels[c].sample_pos += len;
			}
		}
	}
}


/**
 * Sound initialisation routine
 */
bool dr_init_sound()
{
	int sound_ok = 0;
	if(use_sound!=0) {
		return use_sound;	// avoid init twice
	}
	use_sound = 1;

	// initialize SDL sound subsystem
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != -1) {

		// open an audio channel
		SDL_AudioSpec desired;

		desired.freq = 48000;
		desired.channels = 1;
		desired.format = AUDIO_S16SYS;
		desired.samples = 4096;
		desired.userdata = NULL;

		desired.callback = sdl_sound_callback;

		if (SDL_OpenAudio(&desired, &output_audio_format) != -1) {

			// check if we got the right audio format
			// The below seems not to work in Windows 64-bit; but disabling it allows the sound to work correctly.
			//if (output_audio_format.format == AUDIO_S16SYS) {
			if(true) { 
				int i;

				// finished initializing
				sound_ok = 1;

				for (i = 0; i < CHANNELS; i++)
				channels[i].sample = 255;

				// start playing sounds
				SDL_PauseAudio(0);

			} else {
#ifndef _MSC_VER		
				dbg->important("Open audio channel doesn't meet requirements. Muting");
#endif
				SDL_CloseAudio();
				SDL_QuitSubSystem(SDL_INIT_AUDIO);
			}


		} else {
#ifndef _MSC_VER	
			dbg->important("Could not open required audio channel. Muting");
#endif
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
		}
	}
	else {
#ifndef _MSC_VER	
		dbg->important("Could not initialize sound system. Muting");
#endif
	}

	use_sound = sound_ok ? 1: -1;
	return sound_ok;
}


/**
 * loads a sample
 * @return a handle for that sample or -1 on failure
 * @author Hj. Malthaner
 */
int dr_load_sample(const char *filename)
{
	if(use_sound>0  &&  samplenumber < 1024) {

		SDL_AudioSpec wav_spec;
		SDL_AudioCVT  wav_cvt;
		Uint8 *wav_data;
		Uint32 wav_length;
		sample smp;

		/* load the sample */
		if (SDL_LoadWAV(filename, &wav_spec, &wav_data, &wav_length) == NULL) {
#ifndef _MSC_VER	
			dbg->warning("dr_load_sample()", "could not load wav (%s)", SDL_GetError());
#endif
			return -1;
		}

		/* convert the loaded wav to the internally used sound format */
		if (SDL_BuildAudioCVT(&wav_cvt,
			    wav_spec.format, wav_spec.channels, wav_spec.freq,
			    output_audio_format.format,
			    output_audio_format.channels,
			    output_audio_format.freq) < 0) {
#ifndef _MSC_VER	
			dbg->warning("dr_load_sample()", "could not create conversion structure");
#endif
			SDL_FreeWAV(wav_data);
			return -1;
		}

		wav_cvt.buf = MALLOCN(Uint8, wav_length * wav_cvt.len_mult);
		wav_cvt.len = wav_length;
		memcpy(wav_cvt.buf, wav_data, wav_length);

		SDL_FreeWAV(wav_data);

		if (SDL_ConvertAudio(&wav_cvt) < 0) {
#ifndef _MSC_VER	
			dbg->warning("dr_load_sample()", "could not convert wav to output format");
#endif
			return -1;
		}

		/* save the data */
		smp.audio_data = wav_cvt.buf;
		smp.audio_len = wav_cvt.len_cvt;
		samples[samplenumber] = smp;
#ifndef _MSC_VER	
		dbg->message("dr_load_sample", "Loaded %s to sample %i.",filename,samplenumber);
#endif

		return samplenumber++;
	}
	return -1;
}


/**
 * plays a sample
 * @param key the key for the sample to be played
 * @author Hj. Malthaner
 */
void dr_play_sample(int sample_number, int volume)
{
	if(use_sound>0) {

		int c;

		if (sample_number == -1) {
			return;
		}

		/* find an empty channel, and play */
		for (c = 0; c < CHANNELS; c++) {
			if (channels[c].sample == 255) {
				channels[c].sample = sample_number;
				channels[c].sample_pos = 0;
				channels[c].volume = volume * SDL_MIX_MAXVOLUME >> 8;
				break;
			}
		}
	}
}
