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
#include <math.h>
#include <csound/csound.h>

#include "warpy.h"
#include "warpy.orc.c"

struct warpy {
        CSOUND* csound;
};

static bool ensure_status(const int status,
                          const char* const errmsg,
                          CSOUND* csound)
{
        if (status != 0) {
                puts(errmsg);
                fprintf(stderr, "    error %d\n", status);
                csoundDestroy(csound);
                return false;
        } else {
                return true;
        }
}

static const char* KEEP_RUNNING = "i \"PathGetter\" 0 z\n";

struct warpy* start_warpy(void)
{
        struct warpy* warpy = (struct warpy*)malloc(sizeof(struct warpy));

        CSOUND* csound = csoundCreate(NULL);
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
        warpy->csound = csound;
        return warpy;
}

void stop_warpy(struct warpy* warpy)
{
        csoundDestroy(warpy->csound);
        free(warpy);
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

//static MYFLT speed_scale_upper_half(double norm_speed)
//{
//      // 0.5 ->  1,    0.6 ->  2.80, 0.7 ->  6.68,
//      // 0.8 -> 14.19, 0.4 -> 27.59, 0.5 -> 50
//      return (MYFLT)50*(pow(norm_speed,5.643856189774));
//}

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
        csoundSetControlChannel(warpy->csound, "center", center);
}
