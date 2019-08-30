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
#include <csound/csound.h>

#include "warpy.h"

#define CONTROL_PERIOD_FRAMES 64
#define MIDI_MESSAGE_BUFFER_SIZE 4096

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

static struct audio_sample* create_audio_sample(void)
{
        struct audio_sample* audio_sample =
                (struct audio_sample*)malloc(sizeof(struct audio_sample));
        audio_sample->left = 0;
        audio_sample->right = 0;
        return audio_sample;
}

struct bus_args {
        double speed;
        float gain;
        int center;
};

static struct bus_args* create_bus_args(void)
{
        return calloc(1, sizeof(struct bus_args));
}

struct warpy {
        CSOUND* csound;
        int channels;
        struct midi_message_buffer* midi_message_buffer;
        struct audio_sample* audio_sample;
        struct bus_args* bus_args;
        CSOUND_PARAMS* params;
        uint32_t control_period_frames;
        uint32_t audio_buffer_pos;
        bool never_run;
};

static struct warpy* create_warpy(void)
{
        struct warpy* warpy = (struct warpy*)malloc(sizeof(struct warpy));
        warpy->midi_message_buffer = create_midi_message_buffer();
        warpy->audio_sample = create_audio_sample();
        warpy->bus_args = create_bus_args();
        int channels = 2;
        warpy->channels = channels;
        warpy->control_period_frames = CONTROL_PERIOD_FRAMES;
        warpy->audio_buffer_pos = 0;
        warpy->never_run = true;
        return warpy;
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

                if (size) {
                        uint32_t i;
                        for (i = 0; i < size; i++)
                                *buffer++ = message.raw_message[i];
                        clear_midi_message(&midi_buffer[i]);
                        total_bytes += size;
                }
        }
        warpy->midi_message_buffer->pos = 0;

        return total_bytes;
}

static void check_sample_rate(uint64_t* sample_rate)
{
        if (*sample_rate > UINT32_MAX) {
                uint16_t reasonable_sample_rate = 48000;
                fprintf(stderr, "WARN: invalid sample rate of %lu\n", *sample_rate);
                fprintf(stderr, "      setting sample rate to %d\n", reasonable_sample_rate);
                *sample_rate = reasonable_sample_rate;
        }
}

static void set_up_midi(CSOUND* csound)
{
        csoundSetOption(csound, "-+rtmidi=null");
        csoundSetOption(csound, "-M0");
        csoundSetHostImplementedMIDIIO(csound, true);
        csoundSetExternalMidiInOpenCallback(csound, open_input_device);
        csoundSetExternalMidiReadCallback(csound, read_midi_data);
}

static void set_up_audio(CSOUND* csound, bool redirect_audio)
{
        if (redirect_audio) {
                csoundSetHostImplementedAudioIO(csound, 1, 0);
                csoundSetOption(csound, "-n"); // disable writing audio to disk
                csoundSetOption(csound, "-d"); // disable text+graphical output
        }
        else {
                //csoundSetOption(csound, "--ogg");
                csoundSetOption(csound, "--output=test.wav");
        };
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
                       uint64_t sample_rate,
                       uint32_t control_period_frames)
{

        CSOUND_PARAMS* params = (CSOUND_PARAMS*)malloc(sizeof(CSOUND_PARAMS));
        csoundGetParams(csound, params);

        params->sample_rate_override = sample_rate;
        set_ksmps(csound, control_period_frames);
        params->nchnls_override = warpy->channels;
        params->e0dbfs_override = 1;

        csoundSetParams(csound, params);
        warpy->params = params;
}

struct warpy* start_warpy(uint64_t sample_rate, bool redirect_audio)
{
        check_sample_rate(&sample_rate);

        struct warpy* warpy = create_warpy();

        CSOUND* csound = csoundCreate(warpy);

        set_up_midi(csound);
        set_up_audio(csound, redirect_audio);
        set_params(warpy, csound, sample_rate, CONTROL_PERIOD_FRAMES);
        int orcstatus = csoundCompileOrc(csound, WARPY_ORC);
        if (!ensure_status(orcstatus,
                           "Orchestra did not compile\n",
                           csound))
                return NULL;
        int scorestatus = csoundReadScore(csound, KEEP_RUNNING);
        if (!ensure_status(scorestatus,
                           "Problem with dummy score\n",
                           csound))
                return NULL;
        int startstatus = csoundStart(csound);
        if (!ensure_status(startstatus,
                           "Csound failed to start\n",
                           csound))
                return NULL;

        warpy->csound = csound;
        return warpy;
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
        csoundDestroy(warpy->csound);
        free(warpy->params);
        free(warpy);
}

int get_channel_count(struct warpy* warpy)
{
        return warpy->channels;
}

void update_sample_path(struct warpy* warpy, char* path)
{
        csoundSetStringChannel(warpy->csound, "path", path);
}

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

static MYFLT exp_scale_lower_conv(double n,
                                  struct scale from,
                                  struct scale to)
{
        // 0   -> 0,    0.1 -> 0.26, 0.2 -> 0.48,
        // 0.3 -> 0.68, 0.4 -> 0.85, 0.5 -> 1
        double base   = pow(from.midpoint, -(1/from.midpoint));
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

static const struct scale SPEED_SCALE = {
        .floor    = 0.001,
        .ceil     = 50,
        .midpoint = 1
};

void update_speed(struct warpy* warpy, double norm_speed)
{
        if (warpy->bus_args->speed == norm_speed)
                return;
        warpy->bus_args->speed = norm_speed;

        MYFLT speed;
        norm_speed = fabs(norm_speed);

        if (norm_speed > NORM_SCALE.ceil)
                norm_speed = NORM_SCALE.ceil;

        if (norm_speed == NORM_SCALE.floor) {
                speed = SPEED_SCALE.floor;
        }
        else if (norm_speed < NORM_SCALE.midpoint) {
                speed = exp_scale_lower_conv(norm_speed,
                                             NORM_SCALE,
                                             SPEED_SCALE);
        }
        else if (norm_speed > NORM_SCALE.midpoint) {
                speed = exp_scale_upper_conv(norm_speed,
                                             NORM_SCALE,
                                             SPEED_SCALE);
        }
        else {
                speed = SPEED_SCALE.midpoint;
        };

        csoundSetControlChannel(warpy->csound, "speed", speed);
}


void update_gain(struct warpy* warpy, float norm_gain)
{
        norm_gain = fabs(norm_gain);
        if (norm_gain > 1) norm_gain = 1;
        MYFLT gain = (MYFLT)norm_gain * 2;

        csoundSetControlChannel(warpy->csound, "gain", gain);
}

void update_center(struct warpy* warpy, int center)
{
        if (center < 0)
                center = 0;
        if (center > 127) {
                center = 127;
        }

        MYFLT current_center =
                csoundGetControlChannel(warpy->csound, "center", NULL);
        if (current_center != (MYFLT)center)
                csoundSetControlChannel(warpy->csound, "center", center);
}
