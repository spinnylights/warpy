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

struct warpy;

struct warpy* start_warpy(uint64_t sample_rate, bool redirect_audio);
void perform_warpy(struct warpy* warpy);
void stop_warpy(struct warpy* warpy);

void send_midi_message(struct warpy* warpy, uint8_t* raw, uint64_t size);

void update_sample_path(struct warpy* warpy, char* path);
void update_speed(struct warpy* warpy, double norm_speed);
void update_gain(struct warpy* warpy, float norm_gain);
void update_center(struct warpy* warpy, int center);

#endif
