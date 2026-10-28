#include "SoundMixer.h"
#include "SDL_sound.h"
#include <math.h>
#include "stdafx.h"
#include "CSB.h"
#include "Data.h"


struct _Mix_effectinfo
{
	Mix_EffectFunc_t callback;
	Mix_EffectDone_t done_callback;
	void *udata;
	struct _Mix_effectinfo *next;
};

static int reserved_channels = 0;
/* Used to calculate fading steps */
typedef _Mix_effectinfo effect_info;

static effect_info *posteffects = NULL;
static int audio_opened = 0;
SDL_AudioSpec mixer;
static int num_channels;
static const char **chunk_decoders = NULL;
static int num_decoders = 0;

static void (*channel_done_callback)(int channel) = NULL;
static void (*mix_postmix)(void *udata, Uint8 *stream, int len) = NULL;
static void *mix_postmix_data = NULL;

static int _Mix_remove_all_effects(int channel, effect_info **e);


static struct _Mix_Channel {
	Sound_Sample *chunk;
	int decoded_bytes;
	uint8_t *decoded_ptr;
	int playing;
	int paused;
	int volume;
	int looping;
	int tag;
	Uint32 expire;
	Uint32 start_time;
	Mix_Fading fading;
	int fade_volume;
	int fade_volume_reset;
	Uint32 fade_length;
	Uint32 ticks_fade;
	effect_info *effects;
	int posX,posY;
} *mix_channel = NULL;

void set_sound_pos(int i, int posX, int posY) {
    mix_channel[i].posX = posX;
    mix_channel[i].posY = posY;
}

/*
 * rcg06122001 Cleanup effect callbacks.
 *  MAKE SURE SDL_LockAudio() is called before this (or you're in the
 *   audio callback).
 */
static void _Mix_channel_done_playing(int channel)
{
  if (channel_done_callback)
  {
    channel_done_callback(channel);
  };
  /*
   * Call internal function directly, to avoid locking audio from
   *   inside audio callback.
   */
  _Mix_remove_all_effects(channel, &mix_channel[channel].effects);
}


/* Play an audio chunk on a specific channel.
   If the specified channel is -1, play on the first free channel.
   'ticks' is the number of milliseconds at most to play the sample, or -1
   if there is no limit.
   Returns which channel was used to play the sound.
*/
int Mix_PlayChannelTimed(int which, Sound_Sample *chunk, int loops, int ticks)
{
  int i;
  /* Don't play null pointers :-) */
  if ( chunk == NULL )
  {
    Mix_SetError("Tried to play a NULL sample");
    return(-1);
  }
  /* Lock the mixer while modifying the playing channels */
  SDL_LockAudio();
  {
    /* If which is -1, play on the first free channel */
    if ( which == -1 )
    {
      for ( i=reserved_channels; i<num_channels; ++i )
      {
        if ( mix_channel[i].playing <= 0 )
        {
	  break;
        };
      }
      if ( i == num_channels )
      {
        Mix_SetError("No free channels available");
	which = -1;
      }
      else
      {
        which = i;
      }
    }
    /* Queue up the audio data for this channel */
    if ( which >= 0 && which < num_channels )
    {
      Uint32 sdl_ticks = SDL_GetTicks();
      if (Mix_Playing(which))
      {
        _Mix_channel_done_playing(which);
      };
      mix_channel[which].playing = 1;
      mix_channel[which].decoded_bytes = 0;
      mix_channel[which].looping = loops;
      mix_channel[which].chunk = chunk;
      mix_channel[which].paused = 0;
      mix_channel[which].fading = MIX_NO_FADING;
      mix_channel[which].start_time = sdl_ticks;
      mix_channel[which].expire = (ticks>0) ? (sdl_ticks + ticks) : 0;
    }
  }
  SDL_UnlockAudio();
  /* Return the channel on which the sound is being played */
  return(which);
}


int Mix_Playing(int which)
{
  int status;
  status = 0;
  if ( which == -1 )
  {
    int i;
    for ( i=0; i<num_channels; ++i )
    {
      if ((mix_channel[i].playing > 0) ||
				(mix_channel[i].looping > 0))
      {
        ++status;
      };
    };
  }
  else if ( which < num_channels )
  {
    if ( (mix_channel[which].playing > 0) ||
		     (mix_channel[which].looping > 0) )
    {
      ++status;
    };
  };
  return(status);
}

/* MAKE SURE you hold the audio lock (SDL_LockAudio()) before calling this! */
static int _Mix_remove_effect(int channel, effect_info **e, Mix_EffectFunc_t f)
{
  effect_info *cur;
  effect_info *prev = NULL;
  effect_info *next = NULL;
  if (!e)
  {
    Mix_SetError("Internal error");
    return(0);
  };
  for (cur = *e; cur != NULL; cur = cur->next)
  {
    if (cur->callback == f)
    {
      next = cur->next;
      if (cur->done_callback != NULL)
      {
        cur->done_callback(channel, cur->udata);
      };
      SDL_free(cur);
      if (prev == NULL)
      {   /* removing first item of list? */
        *e = next;
      }
       else
      {
        prev->next = next;
      };
      return(1);
    };
    prev = cur;
  };
  Mix_SetError("No such effect registered");
  return(0);
}


/* MAKE SURE you hold the audio lock (SDL_LockAudio()) before calling this! */
int _Mix_UnregisterEffect_locked(int channel, Mix_EffectFunc_t f)
{
  effect_info **e = NULL;
  if (channel == MIX_CHANNEL_POST)
  {
    e = &posteffects;
  }
  else
  {
    if ((channel < 0) || (channel >= num_channels))
    {
      Mix_SetError("Invalid channel number");
      return(0);
    };
    e = &mix_channel[channel].effects;
  };
  return _Mix_remove_effect(channel, e, f);
}

/* MAKE SURE you hold the audio lock (SDL_LockAudio()) before calling this! */
int _Mix_UnregisterAllEffects_locked(int channel)
{
  effect_info **e = NULL;
  if (channel == MIX_CHANNEL_POST)
  {
    e = &posteffects;
  }
  else
  {
    if ((channel < 0) || (channel >= num_channels))
    {
      Mix_SetError("Invalid channel number");
      return(0);
    };
    e = &mix_channel[channel].effects;
  };
  return _Mix_remove_all_effects(channel, e);
}


int Mix_UnregisterAllEffects(int channel)
{
  int retval;
  SDL_LockAudio();
  retval = _Mix_UnregisterAllEffects_locked(channel);
  SDL_UnlockAudio();
  return(retval);
}


/* Should we favor speed over memory usage and/or quality of output? */
int _Mix_effects_max_speed = 0;


void _Mix_InitEffects(void)
{
  _Mix_effects_max_speed = (SDL_getenv(MIX_EFFECTSMAXSPEED) != NULL);
}

static int _Mix_remove_all_effects(int channel, effect_info **e)
{
  effect_info *cur;
  effect_info *next;
  if (!e)
  {
    Mix_SetError("Internal error");
    return(0);
  };
  for (cur = *e; cur != NULL; cur = next)
  {
    next = cur->next;
    if (cur->done_callback != NULL)
    {
      cur->done_callback(channel, cur->udata);
    };
    SDL_free(cur);
  };
  *e = NULL;
  return(1);
}

static void *Mix_DoEffects(int chan, void *snd, int len)
{
  int posteffect = (chan == MIX_CHANNEL_POST);
  effect_info *e = ((posteffect) ? posteffects : mix_channel[chan].effects);
  void *buf = snd;
  if (e != NULL)  /* are there any registered effects? */
  {		  /* if this is the postmix, we can just overwrite the original. */
    if (!posteffect)
    {
      buf = SDL_malloc(len);
      if (buf == NULL)
      {
        return(snd);
      };
      memcpy(buf, snd, len);
    };
    for (; e != NULL; e = e->next)
    {
      if (e->callback != NULL)
      {
        e->callback(chan, buf, len, e->udata);
      };
    };
  };
  /* be sure to SDL_free() the return value if != snd ... */
  return(buf);
}

int Mix_Volume(int which, int volume)
{
  int i;
  int prev_volume = 0;
  if ( which == -1 )
  {
      for ( i=0; i<num_channels; ++i )
      {
	  prev_volume += Mix_Volume(i, volume);
      }
      prev_volume /= num_channels;
  }
  else if ( which < num_channels )
  {
      prev_volume = mix_channel[which].volume;
      if ( volume >= 0 )
      {
	  if ( volume > SDL_MIX_MAXVOLUME )
	  {
	      volume = SDL_MIX_MAXVOLUME;
	  }
	  mix_channel[which].volume = volume;
      }
  }
  return(prev_volume);
}

static void mix_channels(void *udata, Uint8 *stream, int len)
{
    // Uint8 *mix_input;
    int i; // , mixable, volume = SDL_MIX_MAXVOLUME;
    Uint32 sdl_ticks;
    int mixed = 0;

#if SDL_VERSION_ATLEAST(1, 3, 0)
    /* Need to initialize the stream in SDL 1.3+ */
    // emset(stream, mixer.silence, len);
#endif
    /* Mix any playing channels... */
    sdl_ticks = SDL_GetTicks();
    for ( i=0; i<num_channels; ++i )
    {
	if( ! mix_channel[i].paused )
	{
	    if ( mix_channel[i].expire > 0 && mix_channel[i].expire < sdl_ticks )
	    {
		/* Expiration delay for that channel is reached */
		mix_channel[i].playing = 0;
		mix_channel[i].looping = 0;
		mix_channel[i].fading = MIX_NO_FADING;
		mix_channel[i].expire = 0;
		printf("expire\n");
		_Mix_channel_done_playing(i);
	    }
	    else if ( mix_channel[i].fading != MIX_NO_FADING )
	    {
		Uint32 ticks = sdl_ticks - mix_channel[i].ticks_fade;
		printf("fading\n");
		if( ticks > mix_channel[i].fade_length )
		{
		    Mix_Volume(i, mix_channel[i].fade_volume_reset); /* Restore the volume */
		    if( mix_channel[i].fading == MIX_FADING_OUT )
		    {
			mix_channel[i].playing = 0;
			mix_channel[i].looping = 0;
			mix_channel[i].expire = 0;
			_Mix_channel_done_playing(i);
		    };
		    mix_channel[i].fading = MIX_NO_FADING;
		}
		else
		{
		    if( mix_channel[i].fading == MIX_FADING_OUT )
		    {
			Mix_Volume(i, (mix_channel[i].fade_volume * (mix_channel[i].fade_length-ticks))
				/ mix_channel[i].fade_length );
		    }
		    else
		    {
			Mix_Volume(i, (mix_channel[i].fade_volume * ticks) / mix_channel[i].fade_length );
		    }
		}
	    }
	    if ( mix_channel[i].playing > 0 )
	    {
		int index = 0;
		int remaining = len;
		while (mix_channel[i].playing > 0 && index < len)
		{
		    remaining = len - index;
		    // volume = mix_channel[i].volume;
		    //mix_input = (Uint8 *)Mix_DoEffects
		    //    (i, mix_channel[i].samples, mixable);

		    // From SDL_sound, playsample_simple.c callback... !
		    _Mix_Channel *data = &mix_channel[i];
		    Sound_Sample *sample = data->chunk;
		    int bw = 0; /* bytes written to stream this time through the callback */

		    double panLeft,panRight;
		    if (data->posX < 0) {
			panLeft = panRight = 0.5; // sound on party
		    } else {
			int x = data->posX - d.partyX;
			int y = data->posY - d.partyY;
			double dist = sqrt(x*x + y*y);
			if (dist < 1e-4) {
			    panLeft = panRight = 0.5; // sound on party
			} else {
			    switch(d.partyFacing) {
			    case 0: // north
				panRight = 0.5 + x/dist/2;
				break;
			    case 1: // east
				panRight = 0.5 - y/dist/2;
				break;
			    case 2: // south
				panRight = 0.5 - x/dist/2;
				break;
			    case 3:
				panRight = 0.5 + y/dist/2;
				break;
			    }
			    panLeft = 1 - panRight;
			    double att = 1/(dist*dist);
			    panRight *= att;
			    panLeft *= att;
			}
		    }

		    while (bw < remaining)
		    {
			int cpysize;  /* bytes to copy on this iteration of the loop. */

			if (data->decoded_bytes == 0) /* need more data! */
			{
			    /* if there wasn't previously an error or EOF, read more. */
			    if ( ((sample->flags & SOUND_SAMPLEFLAG_ERROR) == 0) &&
				    ((sample->flags & SOUND_SAMPLEFLAG_EOF) == 0) )
			    {
				data->decoded_bytes = Sound_Decode(sample);
				data->decoded_ptr = (uint8_t*)sample->buffer;
			    } /* if */

			    if (data->decoded_bytes == 0)
			    {
				if (data->looping) {
				    data->looping--;
				    Sound_Rewind(sample);
				    continue;
				}

				/* ...there isn't any more data to read! */
				SDL_memset(stream + bw, '\0', remaining - bw);  /* write siremainingce. */
				mix_channel[i].playing = 0;
				break;  /* we're done playback, one way or another. */
			    } /* if */
			} /* if */

			/* we have data decoded and ready to write to the device... */
			cpysize = remaining - bw;  /* remaining - bw == amount device still wants. */
			if (cpysize > (Sint32)data->decoded_bytes)
			    cpysize = (Sint32)data->decoded_bytes;  /* clamp to what we have left. */

			/* if it's 0, next iteration will decode more or decide we're done. */
			if (cpysize > 0)
			{
			    /* write this iteration's data to the device. */
			    int16_t *dest = (int16_t*)(stream + bw);
			    int16_t *src = (int16_t*)data->decoded_ptr;
			    int cp2 = cpysize/2;
			    // Samples decoded on 1 channel, but played on 2
			    if (!mixed) {
				// SDL_memcpy(stream + bw, (Uint8 *) data->decoded_ptr, cpysize);
				for (int n=0; n<cp2; n++) {
				    dest[n*2] = src[n]*panLeft;
				    dest[n*2+1] = src[n]*panRight;
				}
			    } else {
				int min=0,max=0;
				double amp;
				bool no_amp = false;
				for (int n=0; n<cp2; n++) {
				    int dst = dest[n*2] + src[n]*panLeft;
				    if (dst > 0x7fff && dst > max) {
					max = dst;
				    } else if (dst & 0xffff0000 && !(dst & 0x8000) && dst < min) {
					min = dst;
				    }
				}
				if (abs(min) > max) {
				    amp = 32767.0/abs(min);
				} else if (max > 0) {
				    amp = 32767.0/max;
				} else {
				    no_amp = true;
				}
				if (no_amp) {
				    for (int n=0; n<cp2; n++) {
					dest[n*2] = (dest[n*2] + src[n]*panLeft);
					dest[n*2+1] = (dest[n*2+1] + src[n]*panRight);
				    }
				} else {
				    for (int n=0; n<cp2; n++) {
					dest[n*2] = (dest[n*2] + src[n]*panLeft)*amp;
					dest[n*2+1] = (dest[n*2+1] + src[n]*panRight)*amp;
				    }
				}
			    }

			    /* update state for next iteration or callback */
			    bw += cpysize*2;
			    data->decoded_ptr += cpysize;
			    data->decoded_bytes -= cpysize;
			} /* if */
		    } /* while */
		    mixed = 1;
		    index += remaining;
		    if (!mix_channel[i].playing && !mix_channel[i].looping)
		    {
			_Mix_channel_done_playing(i);
		    }
		}
	    }
	}
    }

    /* rcg06122001 run posteffects... */
    Mix_DoEffects(MIX_CHANNEL_POST, stream, len);
    if ( mix_postmix )
    {
	mix_postmix(mix_postmix_data, stream, len);
    }
    if (!mixed)
	memset(stream, mixer.silence, len);
}

static int devs_audio = -1;

/* Open the mixer with a certain desired audio format */
int Mix_OpenAudio()
{
    int i;
    if (audio_opened) {
	printf("Audio: Mix_OpenAudio() called, but audio already opened!\n");
	return 1;
    }
    SDL_AudioSpec desired;
    char *SoundCard = NULL;
    if (devs_audio < 0) devs_audio = SDL_GetNumAudioDevices(0);
    memset(&mixer,0,sizeof(mixer));
    if (SDL_GetDefaultAudioInfo(&SoundCard,&mixer,0) < 0) {
	printf("SDL_GetDefaultAudioInfo failed!\n");
	exit(1);
    }
    printf("Audio: default soundcard %s, freq %d channels %d\n",SoundCard,mixer.freq,mixer.channels);

    /* Set the desired format and frequency */
    desired = mixer;
    desired.callback = mix_channels;
    desired.userdata = NULL;
    desired.format = AUDIO_S16LSB; // a minimum requirement
    int dev;

    /* Accept nearly any audio format */
    if ( (dev=SDL_OpenAudioDevice(NULL,0,&desired, &mixer,SDL_AUDIO_ALLOW_ANY_CHANGE & (~SDL_AUDIO_ALLOW_FORMAT_CHANGE))) <= 0 ) {
	printf("Audio: opening device failed!\n");
	return(-1);
    }

    printf("audio: got %d, %d channels format desired %x got %x\n",mixer.freq,mixer.channels,desired.format,mixer.format);
    num_channels = MIX_CHANNELS;
    mix_channel = (struct _Mix_Channel *) SDL_malloc(num_channels * sizeof(struct _Mix_Channel));

    /* Clear out the audio channels */
    for ( i=0; i<num_channels; ++i ) {
	mix_channel[i].chunk = NULL;
	mix_channel[i].playing = 0;
	mix_channel[i].looping = 0;
	mix_channel[i].volume = SDL_MIX_MAXVOLUME;
	mix_channel[i].fade_volume = SDL_MIX_MAXVOLUME;
	mix_channel[i].fade_volume_reset = SDL_MIX_MAXVOLUME;
	mix_channel[i].fading = MIX_NO_FADING;
	mix_channel[i].tag = -1;
	mix_channel[i].expire = 0;
	mix_channel[i].effects = NULL;
	mix_channel[i].paused = 0;
    }

    _Mix_InitEffects();
    if (!Sound_Init())
	printf("Audio: sdl_sound init failed, expect trouble!\n");

    audio_opened = 1;
    SDL_PauseAudioDevice(dev,0);
    return(0);
}

/* Halt playing of a particular channel */
int Mix_HaltChannel(int which)
{
  int i;
  if ( which == -1 )
  {
    for ( i=0; i<num_channels; ++i )
    {
      Mix_HaltChannel(i);
    };
  }
  else if ( which < num_channels )
  {
    SDL_LockAudio();
    if (mix_channel[which].playing)
    {
      _Mix_channel_done_playing(which);
      mix_channel[which].playing = 0;
      mix_channel[which].looping = 0;
    };
    mix_channel[which].expire = 0;
    if(mix_channel[which].fading != MIX_NO_FADING)
    { /* Restore volume */
      mix_channel[which].volume = mix_channel[which].fade_volume_reset;
    };
    mix_channel[which].fading = MIX_NO_FADING;
    SDL_UnlockAudio();
  };
  return(0);
}

typedef struct _Eff_positionargs
{
    volatile float left_f;
    volatile float right_f;
    volatile Uint8 left_u8;
    volatile Uint8 right_u8;
    volatile float left_rear_f;
    volatile float right_rear_f;
    volatile float center_f;
    volatile float lfe_f;
    volatile Uint8 left_rear_u8;
    volatile Uint8 right_rear_u8;
    volatile Uint8 center_u8;
    volatile Uint8 lfe_u8;
    volatile float distance_f;
    volatile Uint8 distance_u8;
    volatile Sint16 room_angle;
    volatile int in_use;
    volatile int channels;
} position_args;


static position_args **pos_args_array = NULL;
static position_args *pos_args_global = NULL;
static int position_channels = 0;

void _Eff_PositionDeinit(void)
{
  int i;
  for (i = 0; i < position_channels; i++)
  {
    SDL_free(pos_args_array[i]);
  };
  position_channels = 0;
  SDL_free(pos_args_global);
  pos_args_global = NULL;
  SDL_free(pos_args_array);
  pos_args_array = NULL;
}


void _Mix_DeinitEffects(void)
{
  _Eff_PositionDeinit();
}


void Mix_CloseAudio(void)
{
  int i;
  if ( audio_opened )
  {
    if ( audio_opened == 1 )
    {
      for (i = 0; i < num_channels; i++)
      {
        Mix_UnregisterAllEffects(i);
      };
      Mix_UnregisterAllEffects(MIX_CHANNEL_POST);
      Mix_HaltChannel(-1);
      _Mix_DeinitEffects();
      SDL_CloseAudio();
      SDL_free(mix_channel);
      mix_channel = NULL;
      /* rcg06042009 report available decoders at runtime. */
      SDL_free(chunk_decoders);
      chunk_decoders = NULL;
      num_decoders = 0;
    };
    --audio_opened;
  };
}
