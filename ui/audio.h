#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct audio_timer {
	bool playing;
	uint32_t buf;
	uint32_t queued_frames;
	uint32_t sample_rate;
	uint32_t prev_queued;
	int32_t next_adjust;
	int32_t adjustment;
	double pfrequency;
	double start;
	double ms;
};

struct audio;

int32_t audio_init(struct audio **ctx_out, uint32_t sample_rate);
void audio_destroy(struct audio **ctx_out);
int32_t audio_play(struct audio *ctx, struct audio_timer *timer, int16_t *frames, size_t count);

struct audio_timer audio_timer_init(bool playing, uint32_t sample_rate);
void audio_timer_add_frames(struct audio_timer *timer, size_t count);
int32_t audio_timer_rate_adjust(struct audio_timer *timer);
bool audio_timer_past_buffer(struct audio_timer *timer);
