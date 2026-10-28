//
// #include "stdafx.h"
#include "SDL_audio.h"
#include "SDL_version.h"
#include "SDL_sound.h"

#define MIX_EFFECTSMAXSPEED  "MIX_EFFECTSMAXSPEED"
#define MIX_CHANNELS 16 // with conflux3 8 is not enough...
#define MIX_CHANNEL_POST  -2
#define MIX_MAX_VOLUME		128	/* Volume of a chunk */
#define Mix_SetError	SDL_SetError
#define Mix_GetError	SDL_GetError

/* The internal format for an audio chunk */
struct Mix_Chunk {
	int allocated;
	Uint8 *abuf;
	Uint32 alen;
	Uint8 volume;		/* Per-sample volume, 0-128 */
	int keep;
};


/* The different fading types supported */
enum Mix_Fading
{
  MIX_NO_FADING,
  MIX_FADING_OUT,
  MIX_FADING_IN
};

extern SDL_AudioSpec mixer;

#define Mix_PlayChannel(channel,chunk,loops) Mix_PlayChannelTimed(channel,chunk,loops,-1)
/* The same as above, but the sound is played at most 'ticks' milliseconds */
extern DECLSPEC int SDLCALL Mix_PlayChannelTimed(int channel, Sound_Sample *chunk, int loops, int ticks);
typedef void (*Mix_EffectFunc_t)(int chan, void *stream, int len, void *udata);
typedef void (*Mix_EffectDone_t)(int chan, void *udata);
int _Mix_UnregisterAllEffects_locked(int channel);
extern DECLSPEC int SDLCALL Mix_UnregisterAllEffects(int channel);
extern DECLSPEC int SDLCALL Mix_HaltChannel(int channel);
void _Mix_DeinitEffects(void);
extern DECLSPEC int SDLCALL Mix_Volume(int channel, int volume);
extern DECLSPEC int SDLCALL Mix_Playing(int channel);

int Mix_OpenAudio();
void Mix_CloseAudio(void);

