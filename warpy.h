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

#ifndef xda31427f8254d79912f2b7d75300e4a
#define xda31427f8254d79912f2b7d75300e4a

#include <stdint.h>
#include <stdbool.h>

#define VOC_SPEED 0
#define VOC_PITCH 1

struct param;
struct warpy;
struct bounds {
	struct param* start;
	struct param* end;
};


struct audio_sample {
	float left;
	float right;
};

struct envelope {
	float attack_time;
	float attack_shape;
	float decay_time;
	float decay_shape;
	float sustain_level;
	float release_time;
	float release_shape;
};

struct vocoder_settings {
	int   type;
	float adjust;
	float center;
	float lower_scale;
	float upper_scale;
};

struct warpy* create_warpy(double sample_rate);
bool start_warpy(struct warpy* warpy);
void stop_warpy(struct warpy* warpy);
void destroy_warpy(struct warpy* warpy);

void send_midi_message(struct warpy* warpy, uint8_t* raw, uint64_t size);
struct audio_sample gen_sample(struct warpy* warpy);
int get_channel_count(struct warpy* warpy);

void update_sample_path(struct warpy* warpy, char* path);
void update_vocoder_settings(struct warpy* warpy,
                             const struct vocoder_settings settings);
void update_gain(struct warpy* warpy, float norm_gain);
void update_center(struct warpy* warpy, int center, int voc_param);
void update_envelope(struct warpy* warpy, struct envelope env);
void update_reverse(struct warpy* warpy, bool reverse);
void update_loop_times(struct warpy* warpy, unsigned loop_times);

struct bounds get_main_bounds(struct warpy* warpy);
struct bounds get_sustain_bounds(struct warpy* warpy);
struct bounds get_release_bounds(struct warpy* warpy);
void update_start_and_end_points(struct warpy* warpy,
                                 float start,
                                 float end,
                                 struct bounds bounds);

void update_sustain_section(struct warpy* warpy, bool sustain_section);
void update_tie_sustain_end_to_main_end(struct warpy* warpy,
                                        bool tie_sustain_end_to_main_end);
void update_release_section(struct warpy* warpy, bool release_section);
void update_tie_release_start_to_main_end(struct warpy* warpy,
                                          bool tie_release_start_to_main_end);
void update_release_loop_times(struct warpy* warpy, unsigned loop_times);

#endif
