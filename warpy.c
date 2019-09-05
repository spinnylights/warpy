/*
 * This file is part of Warpy.
 *
 * Warpy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Warpy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Warpy.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <csound/csound.h>
#include <sox.h>

#include "warpy.h"

#define CONTROL_PERIOD_FRAMES 64
#define MIDI_MESSAGE_BUFFER_SIZE 4096
#define MIDI_CACHE_LENGTH 256

struct scale {
	const double floor;
	const double ceil;
	const double midpoint;
};

static const struct scale NORM_SCALE = {
	.floor    = 0,
	.ceil     = 1,
	.midpoint = 0.5
};

static const struct scale SPEED_SCALE = {
	.floor    = 0.001,
	.ceil     = 50,
	.midpoint = 1
};

static const struct scale PITCH_SCALE = {
	.floor    = 0.001,
	.ceil     = 100,
	.midpoint = 1
};

static const char WARPY_ORC[] = {
	#include "warpy.orc.xxd"
};

struct midi_message {
	uint8_t* raw_message;
	uint64_t size;
};

struct midi_message_buffer {
	struct midi_message* messages;
	uint32_t size;
	uint32_t pos;
};

static void clear_midi_message(struct midi_message* message)
{
	uint8_t empty[] = { 0x00 };
	message->raw_message = empty;
	message->size = 0;
}

static struct midi_message_buffer* create_midi_message_buffer(void)
{
	struct midi_message_buffer* message_buffer =
	        (struct midi_message_buffer*)
	        malloc(sizeof(struct midi_message_buffer));
	struct midi_message* messages =
	        (struct midi_message*)
	        calloc(MIDI_MESSAGE_BUFFER_SIZE, sizeof(struct midi_message));
	message_buffer->messages = messages;
	message_buffer->size = MIDI_MESSAGE_BUFFER_SIZE;
	message_buffer->pos = 0;
	return message_buffer;
}

static void destroy_midi_message_buffer(struct midi_message_buffer* buffer)
{
	free(buffer->messages);
	free(buffer);
}

static struct audio_sample* create_audio_sample(void)
{
	struct audio_sample* audio_sample =
	        (struct audio_sample*)malloc(sizeof(struct audio_sample));
	audio_sample->left = 0;
	audio_sample->right = 0;
	return audio_sample;
}

struct param {
	MYFLT (*calc)(float);
	const char* channel;
	float arg;
	MYFLT result;
	bool is_cs_current;
};

struct param* create_param(MYFLT (*calc)(float), const char* channel)
{
	struct param* param = (struct param*)malloc(sizeof(struct param));
	param->calc = calc;
	param->channel = channel;
	param->arg = -100;
	param->result = -100;
	param->is_cs_current = false;
	return param;
}

static MYFLT exp_scale_lower_conv(double n)
{
	// 0   -> 0,    0.1 -> 0.26, 0.2 -> 0.48,
	// 0.3 -> 0.68, 0.4 -> 0.85, 0.5 -> 1
	double base   = pow(NORM_SCALE.midpoint, -(1/NORM_SCALE.midpoint));
	double scaled = (1 - (pow(base,-n)));
	return (MYFLT)(scaled * 2);
}

static MYFLT exp_scale_upper_conv(double n,
                                  struct scale from,
                                  struct scale to)
{
	// 0.5 ->  1,    0.6 ->  2.80, 0.7 ->  6.68,
	// 0.8 -> 14.19, 0.4 -> 27.59, 0.5 -> 50
	if (from.midpoint == 0) return (MYFLT)to.midpoint;
	double exp    = log(1/to.ceil) / log(from.midpoint);
	double result = to.ceil * pow(n,exp);
	return (MYFLT)result;
}

static MYFLT calc_speed_adjust(float norm_speed)
{
	MYFLT speed_adjust;
	norm_speed = fabs(norm_speed);

	if (norm_speed > NORM_SCALE.ceil)
		norm_speed = NORM_SCALE.ceil;

	if (norm_speed == NORM_SCALE.floor) {
		speed_adjust = SPEED_SCALE.floor;
	}
	else if (norm_speed < NORM_SCALE.midpoint) {
		speed_adjust = exp_scale_lower_conv(norm_speed);
	}
	else if (norm_speed > NORM_SCALE.midpoint) {
		speed_adjust = exp_scale_upper_conv(norm_speed,
		                             NORM_SCALE,
		                             SPEED_SCALE);
	}
	else {
		speed_adjust = SPEED_SCALE.midpoint;
	};

	return speed_adjust;
}

static double pitch_scale_upper_linear(MYFLT norm_pitch)
{
	double slope = 5;
	return (slope * (norm_pitch - NORM_SCALE.midpoint)) + PITCH_SCALE.midpoint;
}

static double pitch_scale_upper_exp(MYFLT norm_pitch)
{
	double point_6_to_1_point_5 = 0.5711;
	double exp = (log((double)NORM_SCALE.ceil/(double)PITCH_SCALE.ceil) /
	             log(point_6_to_1_point_5));
	return PITCH_SCALE.ceil * pow(norm_pitch, exp);
}

static MYFLT calc_pitch_adjust(float norm_pitch)
{
	MYFLT pitch_adjust;
	norm_pitch = fabs(norm_pitch);

	if (norm_pitch > NORM_SCALE.ceil)
		norm_pitch = NORM_SCALE.ceil;

	if (norm_pitch == NORM_SCALE.floor) {
		pitch_adjust = PITCH_SCALE.floor;
	}
	else if (norm_pitch < NORM_SCALE.midpoint) {
		pitch_adjust = exp_scale_lower_conv(norm_pitch);
	}
	else if (norm_pitch > NORM_SCALE.midpoint) {
		if (norm_pitch <= 0.6) {
			pitch_adjust = pitch_scale_upper_linear(norm_pitch);
		}
		else {
			pitch_adjust = pitch_scale_upper_exp(norm_pitch);
		}
	}
	else {
		pitch_adjust = PITCH_SCALE.midpoint;
	}

	return pitch_adjust;
}

static MYFLT calc_gain(float norm_gain)
{
	norm_gain = fabs(norm_gain);
	if (norm_gain > 1) norm_gain = 1;
	return (MYFLT)norm_gain * 2;
}

static MYFLT check_midi_note_range(float center_arg)
{
	MYFLT center = center_arg;
	if (center < 0)
		center = 0;
	if (center > 127)
		center = 127;
	return center;
}

static MYFLT check_scale_range(float scale_arg)
{
	MYFLT scale = scale_arg;
	if (scale < -4)
		scale = -4;
	if (scale > 4)
		scale = 4;
	return scale;
}

struct cache {
	uint32_t path_int;
	struct param* gain;
	struct param* speed_adjust;
	struct param* speed_center;
	struct param* speed_lower_scale;
	struct param* speed_upper_scale;
	struct param* pitch_adjust;
	struct param* pitch_center;
	struct param* pitch_lower_scale;
	struct param* pitch_upper_scale;
	struct param* env_attack_time;
	struct param* env_attack_shape;
	struct param* env_decay_time;
	struct param* env_decay_shape;
	struct param* env_sustain_level;
	struct param* env_release_time;
	struct param* env_release_shape;
	struct param* reverse;
	struct param* loop_times;
};

struct cache* create_cache(void)
{
	struct cache* cache = (struct cache*)calloc(1, sizeof(struct cache));
	cache->speed_adjust = create_param(&calc_speed_adjust, "speed_adjust");
	cache->speed_center = create_param(&check_midi_note_range,
	                                   "speed_center");
	cache->speed_lower_scale = create_param(&check_scale_range,
	                                        "speed_lower_scale");
	cache->speed_upper_scale = create_param(&check_scale_range,
	                                        "speed_upper_scale");
	cache->pitch_adjust = create_param(&calc_pitch_adjust, "pitch_adjust");
	cache->pitch_center = create_param(&check_midi_note_range,
	                                   "pitch_center");
	cache->pitch_lower_scale = create_param(&check_scale_range,
	                                        "pitch_lower_scale");
	cache->pitch_upper_scale = create_param(&check_scale_range,
	                                        "pitch_upper_scale");
	cache->gain = create_param(&calc_gain, "gain");
	cache->env_attack_time   = create_param(NULL, "env_attack_time");
	cache->env_attack_shape  = create_param(NULL, "env_attack_shape");
	cache->env_decay_time    = create_param(NULL, "env_decay_time");
	cache->env_decay_shape   = create_param(NULL, "env_decay_shape");
	cache->env_sustain_level = create_param(NULL, "env_sustain_level");
	cache->env_release_time  = create_param(NULL, "env_release_time");
	cache->env_release_shape = create_param(NULL, "env_release_shape");
	cache->reverse = create_param(NULL, "reverse");
	cache->loop_times = create_param(NULL, "loop_times");
	return cache;
}

struct warpy {
	CSOUND* csound;
	double sample_rate;
	int channels;
	struct midi_message_buffer* midi_message_buffer;
	bool* midi_cache;
	struct audio_sample* audio_sample;
	CSOUND_PARAMS* params;
	uint32_t control_period_frames;
	uint32_t audio_buffer_pos;
	bool never_run;
	struct cache* cache;
};

struct warpy* create_warpy(double sample_rate)
{
	struct warpy* warpy = (struct warpy*)malloc(sizeof(struct warpy));
	warpy->sample_rate = sample_rate;
	warpy->midi_message_buffer = create_midi_message_buffer();
	warpy->midi_cache = (bool*)calloc(MIDI_CACHE_LENGTH, sizeof(bool));
	warpy->audio_sample = create_audio_sample();
	int channels = 2;
	warpy->channels = channels;
	warpy->control_period_frames = CONTROL_PERIOD_FRAMES;
	warpy->audio_buffer_pos = 0;
	warpy->never_run = true;
	warpy->cache = create_cache();
	warpy->csound = csoundCreate(warpy);
	warpy->params = (CSOUND_PARAMS*)malloc(sizeof(CSOUND_PARAMS));
	return warpy;
}

void update_against_cache(struct warpy* warpy,
                          struct param* param,
                          float new_arg)
{
	if (!(param->arg == new_arg)) {
		param->arg = new_arg;
		if (param->calc)
			param->result = param->calc(new_arg);
		else
			param->result = new_arg;
		param->is_cs_current = false;
	}

	if (!param->is_cs_current) {
		MYFLT cs_current_val =
			csoundGetControlChannel(warpy->csound,
			                        param->channel,
			                        NULL);
		if (param->result == cs_current_val)
			param->is_cs_current = true;
		else
			csoundSetControlChannel(warpy->csound,
			                        param->channel,
			                        param->result);
	}
}

static bool ensure_status(const int status,
                          const char* const errmsg,
                          CSOUND* csound)
{
	if (status != 0) {
		puts(errmsg);
		fprintf(stderr, "    csound error %d\n", status);
		csoundDestroy(csound);
		return false;
	} else {
		return true;
	}
}

static const char* KEEP_RUNNING = "i \"PathGetter\" 0 z\n";

static int open_input_device(CSOUND* csound,
                             void** user_data,
                             const char* dev_name)
{
	*user_data = csoundGetHostData(csound);
	if (!*user_data) {
		fprintf(stderr, "Csound host data is null\n");
		return CSOUND_ERROR;
	}
	return 0;
}

static bool midi_cache_get(struct warpy* warpy,
                           uint8_t* message,
                           uint64_t size)
{
	bool* cache = warpy->midi_cache;
	if (size >= 2 && message[0] == 0x90) {
		uint8_t note = message[1];
		return cache[note];
	}
	return false;
}

static void midi_cache_set(struct warpy* warpy,
                           uint8_t* message,
                           uint64_t size)
{
	bool* cache = warpy->midi_cache;
	if (size >= 2) {
		uint8_t note = message[0];
		if      (message[0] == 0x90)
			cache[note] = true;
		else if (message[0] == 0x80)
			cache[note] = false;
	}
}

static int read_midi_data(CSOUND* csound,
                          void* user_data,
                          unsigned char *buffer,
                          int space_in_buffer)
{
	struct warpy* warpy = (struct warpy*)user_data;
	uint64_t total_bytes = 0;
	struct midi_message* midi_buffer = warpy->midi_message_buffer->messages;
	uint32_t buffer_pos = warpy->midi_message_buffer->pos;
	uint32_t i;
	for (i = 0; i < buffer_pos; i++) {
		struct midi_message message = midi_buffer[i];
		if (message.size > UINT32_MAX) {
			fprintf(stderr, "MIDI messages must fit in 32 bytes\n");
			return -(abs(EINVAL));
		}

		uint32_t size = (uint32_t)message.size;

		if ((int64_t)space_in_buffer - size < 0) {
			fprintf(stderr, "No space left in Csound MIDI buffer\n");
			return -(abs(CSOUND_MEMORY));
		}

		if (size && !midi_cache_get(warpy,
		                            message.raw_message,
		                            message.size)) {
			for (uint32_t i = 0; i < size; i++)
				*buffer++ = message.raw_message[i];
			midi_cache_set(warpy,
			               message.raw_message,
			               message.size);
			clear_midi_message(&midi_buffer[i]);
			total_bytes += size;
		}
	}

	warpy->midi_message_buffer->pos = 0;

	return total_bytes;
}

static void set_up_midi(CSOUND* csound)
{
	csoundSetOption(csound, "-+rtmidi=null");
	csoundSetOption(csound, "-M0");
	csoundSetOption(csound, "--logfile=null");
	csoundSetHostImplementedMIDIIO(csound, true);
	csoundSetExternalMidiInOpenCallback(csound, open_input_device);
	csoundSetExternalMidiReadCallback(csound, read_midi_data);
}

static void set_up_audio(CSOUND* csound)
{
	csoundSetHostImplementedAudioIO(csound, 1, 0);
	csoundSetOption(csound, "-n"); // disable writing audio to disk
	csoundSetOption(csound, "-d"); // daemon mode
	csoundSetOption(csound, "--messagelevel=4"); // warnings only
}

static void set_ksmps(CSOUND* csound, uint32_t ksmps)
{
	if (ksmps == 0) ksmps = 1;
	const unsigned int ksmps_digits = floor(log10(ksmps) + 1);
	const char* const ksmps_flag = "--ksmps=";
	char ksmps_arg[strlen(ksmps_flag) + ksmps_digits + 1];
	sprintf(ksmps_arg, "%s%d", ksmps_flag, ksmps);
	csoundSetOption(csound, ksmps_arg);
}

static void set_params(struct warpy* warpy,
                       CSOUND* csound,
                       uint32_t control_period_frames)
{
	CSOUND_PARAMS* params = warpy->params;
	csoundGetParams(csound, params);

	params->sample_rate_override = warpy->sample_rate;
	set_ksmps(csound, control_period_frames);
	params->nchnls_override = warpy->channels;
	params->e0dbfs_override = 1;

	csoundSetParams(csound, params);
}

bool start_warpy(struct warpy* warpy)
{
	CSOUND* csound = warpy->csound;

	set_up_midi(csound);
	set_up_audio(csound);
	set_params(warpy, csound, CONTROL_PERIOD_FRAMES);
	int orcstatus = csoundCompileOrc(csound, WARPY_ORC);
	if (!ensure_status(orcstatus,
	                   "Orchestra did not compile\n",
	                   csound))
		return false;
	int scorestatus = csoundReadScore(csound, KEEP_RUNNING);
	if (!ensure_status(scorestatus,
	                   "Problem with dummy score\n",
	                   csound))
		return false;
	int startstatus = csoundStart(csound);
	if (!ensure_status(startstatus,
	                   "Csound failed to start\n",
	                   csound))
		return false;

	return true;
}

static void run_warpy(struct warpy* warpy)
{
	csoundPerformKsmps(warpy->csound);
}

struct audio_sample gen_sample(struct warpy* warpy)
{
	struct audio_sample sample;

	if (warpy->never_run) {
		run_warpy(warpy);
		warpy->never_run = false;
	}
	else if (!(warpy->audio_buffer_pos < warpy->control_period_frames)) {
		run_warpy(warpy);
		warpy->audio_buffer_pos = 0;
	}

	sample.left =
	        (float)csoundGetSpoutSample(warpy->csound,
	                                    warpy->audio_buffer_pos,
	                                    0);
	sample.right =
	        (float)csoundGetSpoutSample(warpy->csound,
	                                    warpy->audio_buffer_pos,
	                                    1);
	warpy->audio_buffer_pos++;

	return sample;
}

static bool space_left_in_midi_buffer(struct midi_message_buffer* buffer) {
	return buffer->pos < buffer->size;
}

void send_midi_message(struct warpy* warpy, uint8_t* raw, uint64_t size)
{
	if (!space_left_in_midi_buffer(warpy->midi_message_buffer)) {
		fprintf(stderr,
		        "WARN: No space left in MIDI buffer; \
		        input discarded\n");
		return;
	}
	struct midi_message* messages = warpy->midi_message_buffer->messages;
	uint32_t* pos = &warpy->midi_message_buffer->pos;
	messages[*pos].raw_message = raw;
	messages[*pos].size = size;
	(*pos)++;
}

void stop_warpy(struct warpy* warpy)
{
	csoundCleanup(warpy->csound);
	csoundReset(warpy->csound);
}

void destroy_warpy(struct warpy* warpy)
{
	csoundDestroy(warpy->csound);
	destroy_midi_message_buffer(warpy->midi_message_buffer);
	free(warpy->audio_sample);
	free(warpy->params);
	free(warpy);
}

int get_channel_count(struct warpy* warpy)
{
	return warpy->channels;
}

#define PATH_CHANNEL "path"

void update_sample_dur(struct warpy* warpy, const char* path)
{
	sox_format_t* header = sox_open_read(path, NULL, NULL, NULL);
	if (!header) {
		fprintf(stderr, "Unable to read from %s\n", path);
		return;
	}
	unsigned channels = header->signal.channels;
	if (channels < 1)
		channels = 1;
	sox_rate_t sample_rate = header->signal.rate;
	if (sample_rate < 1)
		sample_rate = 1;
	uint64_t frames = header->signal.length / channels;
	double length_in_secs = (double)frames / sample_rate;
	sox_close(header);

	csoundSetControlChannel(warpy->csound, "sample_dur", length_in_secs);
}

void update_sample_path(struct warpy* warpy, char* path)
{
	uint64_t i = 0;
	char current = path[i];
	uint32_t path_int = 0;
	while (current != '\0'){
		path_int += current;
		if (path_int > UINT32_MAX) {
			path_int = UINT32_MAX;
			fprintf(stderr,
				"WARN: saturating int conversion of %s at %d\n",
				path,
				UINT32_MAX);
			break;
		}
		current = path[i++];
	}

	if (warpy->cache->path_int == path_int)
		return;
	else {
		int64_t cs_path_size =
		        csoundGetChannelDatasize(warpy->csound, PATH_CHANNEL);
		if (cs_path_size) {
			char cs_path[cs_path_size];
			csoundGetStringChannel(warpy->csound, PATH_CHANNEL, cs_path);
			if (!strcmp(path, cs_path)) {
				warpy->cache->path_int = path_int;
				return;
			}
		}
	}

	update_sample_dur(warpy, path);
	csoundSetStringChannel(warpy->csound, PATH_CHANNEL, path);
}

static void update_speed_adjust(struct warpy* warpy, double norm_speed)
{
	update_against_cache(warpy, warpy->cache->speed_adjust, norm_speed);
}

static void update_pitch_adjust(struct warpy* warpy, MYFLT norm_pitch)
{
	update_against_cache(warpy, warpy->cache->pitch_adjust, norm_pitch);
}

void update_vocoder_settings(struct warpy* warpy,
                             const struct vocoder_settings settings)
{
	int   type        = settings.type;
	float adjust      = settings.adjust;
	float center      = settings.center;
	float lower_scale = settings.lower_scale;
	float upper_scale = settings.upper_scale;

	if (type == VOC_SPEED) {
		update_speed_adjust(warpy, adjust);
		update_center(warpy, center, VOC_SPEED);
		update_against_cache(warpy,
		                     warpy->cache->speed_lower_scale,
		                     lower_scale);
		update_against_cache(warpy,
		                     warpy->cache->speed_upper_scale,
		                     upper_scale);
	}
	else if (type == VOC_PITCH) {
		update_pitch_adjust(warpy, adjust);
		update_center(warpy, center, VOC_PITCH);
		update_against_cache(warpy,
		                     warpy->cache->pitch_lower_scale,
		                     lower_scale);
		update_against_cache(warpy,
		                     warpy->cache->pitch_upper_scale,
		                     upper_scale);
	}
}

void update_gain(struct warpy* warpy, float norm_gain)
{
	update_against_cache(warpy, warpy->cache->gain, norm_gain);
}

void update_center(struct warpy* warpy, int center, int voc_param)
{
	if (voc_param == VOC_SPEED)
		update_against_cache(warpy,
		                     warpy->cache->speed_center,
		                     center);
	else if (voc_param == VOC_PITCH)
		update_against_cache(warpy,
		                     warpy->cache->pitch_center,
		                     center);
}

void update_envelope(struct warpy* warpy, struct envelope env)
{
	update_against_cache(warpy,
	                     warpy->cache->env_attack_time,
	                     env.attack_time);
	update_against_cache(warpy,
	                     warpy->cache->env_attack_shape,
	                     env.attack_shape);
	update_against_cache(warpy,
	                     warpy->cache->env_decay_time,
	                     env.decay_time);
	update_against_cache(warpy,
	                     warpy->cache->env_decay_shape,
	                     env.decay_shape);
	update_against_cache(warpy,
	                     warpy->cache->env_sustain_level,
	                     env.sustain_level);
	update_against_cache(warpy,
	                     warpy->cache->env_release_time,
	                     env.release_time);
	update_against_cache(warpy,
	                     warpy->cache->env_release_shape,
	                     env.release_shape);
}

void update_reverse(struct warpy* warpy, bool reverse)
{
	update_against_cache(warpy, warpy->cache->reverse, reverse);
}

void update_loop_times(struct warpy* warpy, unsigned loop_times)
{
	update_against_cache(warpy, warpy->cache->loop_times, loop_times);
}
