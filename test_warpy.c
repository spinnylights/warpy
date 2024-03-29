#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "warpy.h"
#include "test/tinywav/tinywav.h"

#define SAMPLE_RATE 48000

#define note_on(note)  0x90, (note), 0x60
#define note_off(note) 0x80, (note), 0x00

void write_wav(struct warpy* warpy, float* samples, uint64_t size)
{
	TinyWav tw;
	tinywav_open_write(&tw,
	                   get_channel_count(warpy),
	                   SAMPLE_RATE,
	                   TW_FLOAT32,
	                   TW_INTERLEAVED,
	                   "test.wav");
	tinywav_write_f(&tw, samples, size);
	tinywav_close_write(&tw);
}

void play_test(struct warpy* warpy)
{
	uint8_t c4_on[] =  {  note_on(0x28) };
	uint8_t c4_off[] = { note_off(0x28) };
	uint8_t c3_on[] =  {  note_on(0x34) };
	uint8_t c3_off[] = { note_off(0x34) };
	uint8_t c5_on[] =  {  note_on(0x4c) };
	uint8_t c5_off[] = { note_off(0x4c) };
	uint8_t c6_on[] =  {  note_on(0x58) };
	uint8_t c6_off[] = { note_off(0x58) };

	int secs = 18;
	int length = secs * SAMPLE_RATE;
	int note_length = length / 11;
	uint64_t samples_length = length * get_channel_count(warpy);
	float* samples = (float*)calloc(samples_length, sizeof(float));
	int i;
	struct envelope env;
	env.attack_time   = 0.3;
	env.attack_shape  = 0;
	env.decay_time    = 0.3;
	env.decay_shape   = 0;
	env.sustain_level = 0.9;
	env.release_time  = 0.2;
	env.release_shape = 0;

	struct vocoder_settings speed_settings_1;
	speed_settings_1.type   = VOC_SPEED;
	speed_settings_1.adjust = 0.5;
	speed_settings_1.center = 57;
	speed_settings_1.lower_scale = -1;
	speed_settings_1.upper_scale = 1;

	struct vocoder_settings speed_settings_2;
	speed_settings_2.type   = VOC_SPEED;
	speed_settings_2.adjust = 0.5;
	speed_settings_2.center = 57;
	speed_settings_2.lower_scale = -1;
	speed_settings_2.upper_scale = 1;

	struct vocoder_settings pitch_settings_1;
	pitch_settings_1.type   = VOC_PITCH;
	pitch_settings_1.adjust = 0.5;
	pitch_settings_1.center = 57;
	pitch_settings_1.lower_scale = -1;
	pitch_settings_1.upper_scale = 1;

	struct vocoder_settings pitch_settings_2;
	pitch_settings_2.type   = VOC_PITCH;
	pitch_settings_2.adjust = 0.5;
	pitch_settings_2.center = 57;
	pitch_settings_2.lower_scale = -1;
	pitch_settings_2.upper_scale = 1;

	struct bounds main_bounds = get_main_bounds(warpy);
	struct bounds sustain_bounds = get_sustain_bounds(warpy);
	struct bounds release_bounds = get_release_bounds(warpy);
	double sus_start = 0.0;
	for (i = 0; i < length; i++) {
		//if (i > 0 * SAMPLE_RATE){
		//	speed_settings_1.adjust += 0.00000045;
		//	pitch_settings_1.adjust += 0.00000045;
		//	sus_start += 0.0000001;
		//}
		update_start_and_end_points(warpy,
					    0.0,
					    1,
					    main_bounds);
		update_start_and_end_points(warpy,
					    sus_start,
					    1,
					    sustain_bounds);
		update_start_and_end_points(warpy,
					    0,
					    1.0,
					    release_bounds);
		update_release_section(warpy, false);
		update_envelope(warpy, env);
		if ((double)i < (double)note_length*4.9) {
			update_bpm(warpy, 1);
			update_gain(warpy, 0.3);
			update_reverse(warpy, false);
			update_sample_path(warpy, "tpt_57_med.wav");
			update_vocoder_settings(warpy, speed_settings_1);
			update_vocoder_settings(warpy, pitch_settings_1);
			update_loop_times(warpy, 0);
			update_sustain_section(warpy, true);
			update_release_loop_times(warpy, 0);
			update_vibrato_amp(warpy, 0);
			update_vibrato_waveform_type(warpy, 2);
			update_vibrato_tempo_toggle(warpy, true);
			update_vibrato_freq(warpy, 0.50);
			update_vibrato_tempo_fraction(warpy, 1);
			update_chorus_voices(warpy, 0);
			update_chorus_mix(warpy, 0);
			update_chorus_detune(warpy, 0);
			update_chorus_stereo_spread(warpy, 0);
			update_note_pan_center(warpy, 57);
			update_note_pan_amount(warpy, 1);
		}
		//else if (i < note_length*5) {
		else {
			update_bpm(warpy, 1);
			update_gain(warpy, 0.3);
			update_reverse(warpy, false);
			update_sample_path(warpy, "tpt_57_med.wav");
			update_vocoder_settings(warpy, speed_settings_2);
			update_vocoder_settings(warpy, pitch_settings_2);
			update_loop_times(warpy, 0);
			update_sustain_section(warpy, false);
			update_release_loop_times(warpy, 0);
			update_chorus_voices(warpy, 0);
			update_chorus_mix(warpy, 0);
			update_chorus_detune(warpy, 0);
			update_chorus_stereo_spread(warpy, 0);
		}
		//else {
		//	update_gain(warpy, 0.9);
		//	update_reverse(warpy, false);
		//	update_sample_path(warpy, "tpt_57_48k.wav");
		//	update_vocoder_settings(warpy, speed_settings_2);
		//	update_vocoder_settings(warpy, pitch_settings_2);
		//	update_loop_times(warpy, 1);
		//	update_release_loop_times(warpy, 1);
		//}

		if      (i == note_length)
			send_midi_message(warpy, c4_on, 3);
		else if (i == note_length*2  - 1)
			send_midi_message(warpy, c4_off, 3);
		else if (i == note_length*2)
			send_midi_message(warpy, c3_on, 3);
		else if (i == note_length*3 - 1)
			send_midi_message(warpy, c3_off, 3);
		else if (i == note_length*3)
			send_midi_message(warpy, c5_on, 3);
		else if (i == note_length*4 - 1)
			send_midi_message(warpy, c5_off, 3);
		else if (i == note_length*4)
			send_midi_message(warpy, c6_on, 3);
		else if (i == note_length*5 - 1)
			send_midi_message(warpy, c6_off, 3);
		else if (i == note_length*6)
			send_midi_message(warpy, c4_on, 3);
		else if (i == note_length*7 - 1)
			send_midi_message(warpy, c4_off, 3);
		else if (i == note_length*7)
			send_midi_message(warpy, c3_on, 3);
		else if (i == note_length*8 - 1)
			send_midi_message(warpy, c3_off, 3);
		else if (i == note_length*8)
			send_midi_message(warpy, c5_on, 3);
		else if (i == note_length*9 - 1)
			send_midi_message(warpy, c5_off, 3);
		else if (i == note_length*9)
			send_midi_message(warpy, c6_on, 3);
		else if (i == note_length*10 - 1)
			send_midi_message(warpy, c4_off, 3);

		struct audio_sample sample = gen_sample(warpy);
		int sample_addr = i * get_channel_count(warpy);
		samples[sample_addr]   = sample.left;
		samples[sample_addr+1] = sample.right;
	}

	write_wav(warpy, samples, length);
	free(samples);
}

int main(int argc, char* argv[]) {
	struct warpy* warpy = create_warpy(SAMPLE_RATE);
	bool result = start_warpy(warpy);
	if (result) play_test(warpy);
	stop_warpy(warpy);
	destroy_warpy(warpy);

	return 0;
}
