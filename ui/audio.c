#include "audio.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "SDL2/SDL.h"

#include "../src/nes.h"

#define NEXT_ADJUST  lrint(60.0 * 1.5)
#define ADJUST_INT   50
#define CHANNELS     2

struct audio {
	struct audio_timer timer;
	SDL_AudioDeviceID dev;
};

int32_t audio_init(struct audio **ctx_out, uint32_t sample_rate)
{
	struct audio *ctx = *ctx_out = calloc(1, sizeof(struct audio));

	int32_t r = 0;

	#if defined(_WIN32) || defined(__APPLE__)
	for (int32_t x = 0; x < SDL_GetNumAudioDrivers(); x++) {
		const char *driver_name = SDL_GetAudioDriver(x);

		if (!strcmp(driver_name, "directsound") || !strcmp(driver_name, "coreaudio")) {
			int32_t e = SDL_AudioInit(driver_name);
			if (e != 0) {r = -1; goto except;}

			break;
		}
	}
	#endif

	SDL_AudioSpec want = {0};
	want.freq = sample_rate;
	want.format = AUDIO_S16;
	want.channels = CHANNELS;
	want.samples = 1024;

	SDL_AudioSpec have = {0};
	ctx->dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (ctx->dev == 0) {r = -1; goto except;}

	except:

	if (r != 0)
		audio_destroy(ctx_out);

	return r;
}

void audio_destroy(struct audio **ctx_out)
{
	if (ctx_out == NULL || *ctx_out == NULL)
		return;

	struct audio *ctx = *ctx_out;

	if (ctx->dev != 0) {
		SDL_PauseAudioDevice(ctx->dev, 1);
		SDL_Delay(100);
		SDL_CloseAudioDevice(ctx->dev);
	}

	free(ctx);
	*ctx_out = NULL;
}

int32_t audio_play(struct audio *ctx, struct audio_timer *timer, int16_t *frames, size_t count)
{
	uint32_t queued_bytes = SDL_GetQueuedAudioSize(ctx->dev);
	uint32_t queued_frames = (queued_bytes / sizeof(int16_t)) / CHANNELS;

	// let the buffer accumulate before playing
	if (queued_frames > timer->buf && !timer->playing) {
		SDL_PauseAudioDevice(ctx->dev, 0);
		timer->playing = true;
	}

	int32_t e = SDL_QueueAudio(ctx->dev, frames, (Uint32) (count  * sizeof(int16_t) * CHANNELS));

	return (e == 0) ? 0 : -1;
}


/*** TIMING / SAMPLE RATE ***/

struct audio_timer audio_timer_init(bool playing, uint32_t sample_rate)
{
	struct audio_timer timer;
	memset(&timer, 0, sizeof(struct audio_timer));

	timer.buf = (sample_rate / 100) * 10; // 100 ms
	timer.sample_rate = sample_rate;
	timer.playing = playing;
	timer.pfrequency = (double) SDL_GetPerformanceFrequency();

	return timer;
}

void audio_timer_add_frames(struct audio_timer *timer, size_t count)
{
	timer->ms += 1000.0 * ((double) count / (double) timer->sample_rate);

	double counter = (double) SDL_GetPerformanceCounter();

	if (timer->start > 0.1)
		timer->ms -= 1000.0 * (counter - timer->start) / timer->pfrequency;

	if (timer->ms < 0.0)
		timer->ms = 0.0;

	timer->start = counter;
	timer->queued_frames = lrint((timer->ms / 1000.0) * (double) timer->sample_rate);
}

int32_t audio_timer_rate_adjust(struct audio_timer *timer)
{
	if (timer->playing && timer->next_adjust++ > NEXT_ADJUST) {
		timer->adjustment += (timer->queued_frames > timer->buf && timer->prev_queued < timer->queued_frames) ? -ADJUST_INT :
			(timer->queued_frames < timer->buf && timer->prev_queued > timer->queued_frames) ? ADJUST_INT : 0;
		timer->next_adjust = 0;
		timer->prev_queued = timer->queued_frames;
	}

	return timer->adjustment;
}

bool audio_timer_past_buffer(struct audio_timer *timer)
{
	return timer->queued_frames > timer->buf;
}
