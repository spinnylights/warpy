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

gistereo init 0
gisampleready init 0

gSpath init ""

instr PathGetter
	gSpath chnget "path"

	knewfile changed gSpath
	if knewfile == 1 then
		event "i", "FileLoader", 0, 0
	endif
endin

instr +FileLoader
	ichannels filenchnls gSpath
	gistereo = ichannels - 1
	gileftchan ftgen 1, 0, 0, 1, gSpath, 0, 0, 1
	if gistereo == 1 then
		girightchan ftgen 2, 0, 0, 1, gSpath, 0, 0, 2
	endif
	gisampleready = 1
endin

instr PlayMidi
	if gisampleready == 1 then
		kspeed chnget "speed"

		kgain chnget "gain"
		iamp  ampmidi 1

		kcenter chnget "center"
		imfreq cpsmidi
		kpitch = imfreq/cpsmidinn(kcenter)

		aenv linsegr 0, 0.01, 1, 0.01, 0
		if gistereo == 1 then
			asigl temposcal kspeed, iamp*kgain, kpitch, gileftchan, 1
			asigr temposcal kspeed, iamp*kgain, kpitch, girightchan, 1
				outs asigl*aenv, asigr*aenv
		else
			asig temposcal kspeed, iamp*kgain, kpitch, gileftchan, 1
				outs asig*aenv, asig*aenv
		endif
	endif
endin
