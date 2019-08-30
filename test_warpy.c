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
        uint8_t c4_on[] =  {  note_on(0x3c) };
        uint8_t c4_off[] = { note_off(0x3c) };
        uint8_t c3_on[] =  {  note_on(0x30) };
        uint8_t c3_off[] = { note_off(0x30) };
        uint8_t c5_on[] =  {  note_on(0x48) };
        uint8_t c5_off[] = { note_off(0x48) };
        uint8_t c6_on[] =  {  note_on(0x54) };
        uint8_t c6_off[] = { note_off(0x54) };

        int secs = 14;
        int length = secs * SAMPLE_RATE;
        int note_length = length / 11;
        uint64_t samples_length = length * get_channel_count(warpy);
        float* samples = (float*)calloc(samples_length, sizeof(float));
        int i;
        for (i = 0; i < length; i++) {
                update_sample_path(warpy, "kickroll.wav");
                update_gain(warpy, 0.5);
                if (i < note_length*5) {
                        update_speed(warpy, 0.05);
                        update_center(warpy, 52);
                } else {
                        update_speed(warpy, 0.9);
                        update_center(warpy, 25);
                }

                if      (i == note_length)
                        send_midi_message(warpy, c4_on, 3);
                else if (i == note_length*2 - 1)
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
                        send_midi_message(warpy, c6_off, 3);

                struct audio_sample sample = gen_sample(warpy);
                int sample_addr = i * get_channel_count(warpy);
                samples[sample_addr]   = sample.left;
                samples[sample_addr+1] = sample.right;
        }

        write_wav(warpy, samples, length);
}

int main(int argc, char* argv[]) {
        struct warpy* warpy = start_warpy(SAMPLE_RATE, true);
        play_test(warpy);
        stop_warpy(warpy);

        return 0;
}
