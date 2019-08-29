#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "warpy.h"

#define note_on(note)  0x90, (note), 0x60
#define note_off(note) 0x80, (note), 0x00

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

        int i;
        int goal = 3000;
        for (i = 0; i < goal; i++) {
                if (i == 1) {
                        update_sample_path(warpy, "kickroll.wav");
                        update_speed(warpy, 0.5);
                        update_gain(warpy, 0.5);
                        update_center(warpy, 60);
                }
                else if (i == 250)
                        send_midi_message(warpy, c4_on, 3);
                else if (i == 437)
                        send_midi_message(warpy, c4_off, 3);
                else if (i == 438)
                        send_midi_message(warpy, c3_on, 3);
                else if (i == 624)
                        send_midi_message(warpy, c3_off, 3);
                else if (i == 625)
                        send_midi_message(warpy, c5_on, 3);
                else if (i == 812)
                        send_midi_message(warpy, c5_off, 3);
                else if (i == 813)
                        send_midi_message(warpy, c6_on, 3);
                else if (i == 999)
                        send_midi_message(warpy, c6_off, 3);
                else if (i == 1000) {
                        update_speed(warpy, 0.9);
                        update_center(warpy, 30);
                }
                else if (i == 1250)
                        send_midi_message(warpy, c4_on, 3);
                else if (i == 1437)
                        send_midi_message(warpy, c4_off, 3);
                else if (i == 1438)
                        send_midi_message(warpy, c3_on, 3);
                else if (i == 1624)
                        send_midi_message(warpy, c3_off, 3);
                else if (i == 1625)
                        send_midi_message(warpy, c5_on, 3);
                else if (i == 1812)
                        send_midi_message(warpy, c5_off, 3);
                else if (i == 1813)
                        send_midi_message(warpy, c6_on, 3);
                else if (i == 1999)
                        send_midi_message(warpy, c6_off, 3);

                perform_warpy(warpy);
        }
}

int main(int argc, char* argv[]) {
        int sample_rate = 48000;
        struct warpy* warpy = start_warpy(sample_rate, false);
        play_test(warpy);
        stop_warpy(warpy);

        return 0;
}
